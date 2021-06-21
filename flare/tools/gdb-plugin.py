#!/usr/bin/env python3

# Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
#
# Licensed under the BSD 3-Clause License (the "License"); you may not use this
# file except in compliance with the License. You may obtain a copy of the
# License at
#
# https://opensource.org/licenses/BSD-3-Clause
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Usage:

1. Import the plugin into GDB (execute in GDB's CLI):
   `source /path/to/gdb-plugin.py`

2. The plugin is now usable as (execute in GDB's CLI):
   - `list-fibers`
   - `list-fibers-compact`
   - ...

Both live debugging and core dump analysis are supported.

@sa: https://sourceware.org/gdb/onlinedocs/gdb/Python-API.html
"""

from __future__ import print_function
import struct
import gdb

PAGE_SIZE = 4096  # TODO(luobogao): Do not hardcode here.
MAX_CALL_STACK_DEPTH = 100
INVALID_INSTRUCTION_POINTER = -1

FIBER_STATE_DEAD = 3  # flare::fiber::detail::FiberState::Dead
# @sa: flare/fiber/detail/stack_allocator.h
STACK_TOP_ALIGNMENT = 1 * 1024 * 1024
# @sa: flre/fiber/detail/constants.h
FIBER_STACK_RESERVED_SIZE = 512

# GDB 7.2 does not have a dedicated gdb.MemoryError
GdbMemoryError = gdb.MemoryError if hasattr(gdb,
                                            'MemoryError') else gdb.GdbError
COMMAND_USER = gdb.COMMAND_USER if hasattr(gdb,
                                           'COMMAND_USER') else gdb.COMMAND_NONE


def _cast_to_int(value):
    """Cast `gdb.Value` to `int`."""
    # GDB 7.2 cannot cast `gdb.Value` to `int` directly.
    try:
        return int(value.cast(gdb.lookup_type('long')))
    except gdb.error:
        return int(value.cast(gdb.lookup_type('int')))


def _get_inferior():
    """Returns current inferior."""
    # GDB 7.2 does not support `gdb.selected_inferior()`, in this case we use
    # `gdb.inferiors()[0]` as a workaround.
    if not hasattr(gdb, 'selected_inferior'):
        return gdb.inferiors()[0]
    return gdb.selected_inferior()


def _get_newest_frame():
    """Returns newest frame."""
    # GDB 7.2 does not support `gdb.newest_frame()`, in this case we mimic it.
    if not hasattr(gdb, 'newest_frame'):
        frame = gdb.selected_frame()
        while frame.newer() is not None:
            frame = frame.newer()
        return frame
    return gdb.newest_frame()


def _read_register_from_frame(frame, reg):
    """Read register value from a given frame."""
    if not hasattr(frame, 'read_register'):  # GDB 7.6 does not support this.
        was = gdb.selected_frame()
        try:
            frame.select()
            return _cast_to_int(gdb.parse_and_eval('$' + reg))
        finally:
            was.select()
    else:
        return _cast_to_int(frame.read_register(reg))


class MemorySegment(object):

    def __init__(self, start, end, objfile):
        self.start, self.end, self.objfile = start, end, objfile
        self.size = self.end - self.start


class Frame(object):
    """`gdb.Frame` goes out of scope once "current" thread changes.

    So we make a copy of it here.
    """

    def __init__(self, frame):
        self.mxcsr = _read_register_from_frame(frame, 'mxcsr')
        self.x87cw = 0  # TODO(luobogao): x87 control word
        self.r12 = _read_register_from_frame(frame, 'r12')
        self.r13 = _read_register_from_frame(frame, 'r13')
        self.r14 = _read_register_from_frame(frame, 'r14')
        self.r15 = _read_register_from_frame(frame, 'r15')
        self.rbx = _read_register_from_frame(frame, 'rbx')
        self.rbp = _read_register_from_frame(frame, 'rbp')
        self.rip = _read_register_from_frame(frame, 'rip')
        self.rsp = _read_register_from_frame(frame, 'rsp')


class Fiber(object):
    """This class represent an alive fiber in debuggee."""

    def __init__(self, inferior, fiber_entity_ptr):
        # Here we read more than what we need. We actually only need to read
        # `FiberEntity`, the rest bytes (e.g., the magic) is not needed. But
        # reading more bytes won't hurt.
        fiber_entity = inferior.read_memory(fiber_entity_ptr,
                                            FIBER_STACK_RESERVED_SIZE)

        self.id = self._read_field(fiber_entity, 'Q', 'debugging_fiber_id')
        self.state = self._read_field(fiber_entity, 'I', 'state')
        state_save_area = self._read_field(fiber_entity, 'P', 'state_save_area')
        self.stack_top = fiber_entity_ptr
        self.stack_bottom = fiber_entity_ptr - self._read_field(
            fiber_entity, 'Q', 'stack_size')

        # Tests if the fiber has started running.
        #
        # For the moment this always holds. Not-yet-started fibers should have
        # no stack allocated to them.
        self.started = True

        try:
            saved_state = inferior.read_memory(state_save_area, 0x40)
        except GdbMemoryError:
            saved_state = '\x00' * 0x40
        # @sa: flare/fiber/detail/x86_64/jump_context.S
        #
        # +---------------------------------------------------------------+
        # | 0x0   | 0x4   | 0x8   | 0xc   | 0x10  | 0x14  | 0x18  | 0x1c  |
        # |-------+-------+---------------+---------------+---------------|
        # | mxcsr | x87cw |      R12      |      R13      |      R14      |
        # |-------+-------+---------------+---------------+---------------|
        # | 0x20  | 0x24  | 0x28  | 0x2c  | 0x30  | 0x34  | 0x38  | 0x3c  |
        # |---------------+---------------+---------------+---------------|
        # |      R15      |      RBX      |      RBP      |      RIP      |
        # +---------------------------------------------------------------+
        self.mxcsr = struct.unpack('I', saved_state[0x00:0x04])[0]
        self.x87cw = struct.unpack('I', saved_state[0x04:0x08])[0]
        self.r12 = struct.unpack('Q', saved_state[0x08:0x10])[0]
        self.r13 = struct.unpack('Q', saved_state[0x10:0x18])[0]
        self.r14 = struct.unpack('Q', saved_state[0x18:0x20])[0]
        self.r15 = struct.unpack('Q', saved_state[0x20:0x28])[0]
        self.rbx = struct.unpack('Q', saved_state[0x28:0x30])[0]
        self.rbp = struct.unpack('Q', saved_state[0x30:0x38])[0]
        self.rip = struct.unpack('Q', saved_state[0x38:0x40])[0]
        self.rsp = state_save_area + 0x38 + 8

    def retrieve_state_from(self, frame):
        """Use state from `frame` to overwrite what we currently have."""
        self.mxcsr = frame.mxcsr
        self.x87cw = frame.x87cw
        self.r12 = frame.r12
        self.r13 = frame.r13
        self.r14 = frame.r14
        self.r15 = frame.r15
        self.rbx = frame.rbx
        self.rbp = frame.rbp
        self.rip = frame.rip
        self.rsp = frame.rsp

    def _read_field(self, fiber_entity, fmt, field):
        if not hasattr(Fiber, 'offsets'):
            Fiber.offsets = {}
        if field not in Fiber.offsets:
            Fiber.offsets[field] = Fiber._get_fiber_entity_field_offset(field)
        size = struct.calcsize(fmt)
        return struct.unpack(
            fmt,
            fiber_entity[Fiber.offsets[field]:Fiber.offsets[field] + size])[0]

    @staticmethod
    def _get_fiber_entity_field_offset(name):
        expr = "&(('flare::fiber::detail::FiberEntity'*)0x0)->{0}".format(name)
        try:
            field_offset = _cast_to_int(gdb.parse_and_eval(expr))
            return field_offset
        except gdb.error as xcpt:
            raise RuntimeError("Cannot resolve `FiberEntity`'s fields [{0}]. "
                               "Do you have debugging symbols available? "
                               "Expr: [{1}], Error [{2}].".format(
                                   name, expr, str(xcpt)))

    @staticmethod
    def _get_constant(name):
        if not hasattr(Fiber, name):
            v = 0
            try:
                v = _cast_to_int(gdb.parse_and_eval(name))
            except gdb.error:
                pass  # Nothing
            Fiber.fiber_magic_size = v
        return Fiber.fiber_magic_size


# Note that `info proc mappings` does not work correctly in core-dump.
#
# We can use `info files` in this case. Anonymous mappings are treated as data
# file by GDB in core-dump.
#
# This method does not work in live debugging, unfortunately.
#
# Therefore, we provide two different method for get memory segments, one for
# live debugging, and one for core-dump analysis.


def _is_live_debugging():
    """Test if we're in a live debugging session or core-dump analysis."""
    # In case of live debugging, `info proc` returns process's ID in first line
    # of its output.
    #
    # (gdb) info proc
    # process 1
    # cmdline = '/usr/bin/cat'
    return 'process ' in gdb.execute('info proc', to_string=True).split('\n')[0]


def _get_memory_maps_live_debugging():
    """Read memory map in live debugging environment."""
    # (gdb) info proc map
    # process 1
    # Mapped address spaces:
    #
    #           Start Addr           End Addr       Size     Offset objfile
    #             0x400000           0x409000     0x9000        0x0 /path/to/...
    maps = gdb.execute('info proc mappings', to_string=True).strip().split('\n')
    while map and 'Start Addr' not in maps[0]:
        del maps[0]
    del maps[0]

    segs = []
    for seg in maps:
        splited = seg.split()
        start, end = [int(x, 16) for x in splited[:2]]
        if len(splited) == 5 and not splited[-1].startswith('[stack:'):
            objfile = splited[-1]
        else:
            objfile = ''
        segs.append(MemorySegment(start, end, objfile))
    return segs


def _get_memory_maps_in_core_dump():
    """Read memory map in core dump analysis environment."""
    # (gdb) info files
    # Symbols from "/usr/bin/cat".
    # Local core dump file:
    #     `core.1', file type elf64-x86-64.
    #     0x0000000000400000 - 0x000000000040b000 is load1
    #     0x000000000060b000 - 0x000000000060c000 is load2
    #     0x000000000060c000 - 0x000000000060d000 is load3
    #     0x000000000060d000 - 0x000000000062e000 is load4
    #     0x00007ffff7a15000 - 0x00007ffff7a16000 is load5
    #     0x00007ffff7a16000 - 0x00007ffff7a17000 is load6
    #     0x00007ffff7dd0000 - 0x00007ffff7dd4000 is load7
    #     0x00007ffff7dd4000 - 0x00007ffff7dd6000 is load8
    #     0x00007ffff7dd6000 - 0x00007ffff7ddb000 is load9
    # Local exec file:
    #     `/usr/bin/cat', file type elf64-x86-64.
    #     Entry point: 0x402644
    #     0x0000000000400238 - 0x0000000000400254 is .interp
    #     0x0000000000400254 - 0x0000000000400274 is .note.ABI-tag
    files = gdb.execute('info files', to_string=True).strip().split('\n')
    while files and 'Local core dump file' not in files[0]:
        del files[0]
    del files[0]
    del files[0]  # "`core.1', file type elf64-x86-64."

    segs = []
    for seg in files[3:]:  # First 3 lines are headers.
        if seg.startswith('Local exec file:'):
            # Executable sections are hard to parse, and of no use to us. So we
            # skip them.
            break  # TODO(luobogao): Parse executable sections.

        splited = seg.split()
        start = int(splited[0], 16)
        end = int(splited[2], 16)
        objfile = splited[-1] if not splited[-1].startswith('load') else ''
        segs.append(MemorySegment(start, end, objfile))
    return segs


def get_memory_maps():
    """List memory mappings in process."""
    if _is_live_debugging():
        return _get_memory_maps_live_debugging()
    return _get_memory_maps_in_core_dump()


def _round_up(value, alignment):
    """Round `value` up to a multiple of `alignment`."""
    return (value + (alignment - 1)) // alignment * alignment


# @sa: https://fy.blackhats.net.au/blog/html/2017/08/04/so_you_want_to_script_gdb_with_python.html
def get_thread_frames(inferior):
    """Enumerate pthreads' newest frame."""
    frames = []
    was = gdb.selected_thread()
    try:
        for thread in inferior.threads():
            thread.switch()
            frames.append(Frame(_get_newest_frame()))
    finally:
        was.switch()  # Switch thread back.
    return frames


def try_extract_fiber(inferior, active_frames, offset):
    """Inspect memory region pointed to by `offset`. If there resides a control
    structure of an "alive" fiber, a `Fiber` object representing that fiber is
    returned.
    """
    try:
        # It's a fiber stack. Otherwise we shouldn't have been called.
        fiber = Fiber(inferior, offset - FIBER_STACK_RESERVED_SIZE)

        if fiber.stack_top == fiber.stack_bottom:
            # Master fiber.
            #
            # However, this branch shouldn't be taken, as master fiber's control
            # block is unlikely to be 1MB aligned.
            return None
        if fiber.state == FIBER_STATE_DEAD:
            return None  # Dead fiber.
        if not fiber.started:
            return None  # Not started yet. Its stack is in a mass.

        # Let's see if this fiber is currently running.
        for frame in active_frames:
            if fiber.stack_bottom <= frame.rsp <= fiber.stack_top:
                # Yes. Update its context from corresponding pthread worker's
                # frame then.
                fiber.retrieve_state_from(frame)

                # There's a caveat, though: If the fiber is being swapped in /
                # out, the pthread worker's frame might or might not reflect the
                # fiber's state (e.g., `jump_context` has swapped `rsp`, but not
                # `rip` / `rbp` / ... yet.). In this case the backtrace can be
                # wrong.
                #
                # We'll issue a warning in `describe_call_stack()` in this case.
                #
                # Better approach would be inspecting the instruction pointer.
                # Except for the case when the fiber is being swapped out and
                # its state has not been fully saved, in which case the pthread
                # worker's frame should be used instead, we can always use state
                # in the state save area.
                break

        return fiber
    except GdbMemoryError:
        return None


def get_fibers(inferior):
    """Returns: Collection of `Fiber`s."""
    segs = get_memory_maps()

    # For running fibers, the "saved" state does not reflect their current
    # state. Here we enumerate all running pthread's newest frames. For each
    # fiber we find below, if it's stack is being used by a pthread, we'll use
    # the pthread's context to "represent" the fiber's state instead.
    active_frames = get_thread_frames(inferior)

    # All stacks ever allocated (except those already destroyed) are registered
    # with the stack registry.
    #
    # Therefore we can simply enumerate elements in the registry.
    stacks_ptr, count = struct.unpack(
        'QQ',  # I Love QQ.
        inferior.read_memory(
            gdb.parse_and_eval("&'flare::fiber::detail::stack_registry'"), 16))
    stacks = struct.unpack('Q' * count,
                           inferior.read_memory(stacks_ptr, 8 * count))

    print('Found %d stacks in total, checking for fiber aliveness. '
          'This may take a while.' % len(stacks))
    print('')

    # TODO(luobogao): We might want to print a progress bar here.
    for stack in stacks:
        if stack == 0:  # Empty slot then.
            continue

        # Let's see if there's a fiber control block.
        fiber = try_extract_fiber(inferior, active_frames, stack)
        if fiber != None:
            yield fiber

    return


def describe_instruction_pointer(inst_ptr):
    """Use a human readable string (normally its corresponding function name)
    to describe an instruction pointer.
    """
    if inst_ptr == INVALID_INSTRUCTION_POINTER:
        return 'Invalid instruction pointer, corrupted stack?'

    file_pos = '(Not supported by GDB 7.2)'
    if hasattr(gdb, 'find_pc_line'):  # GDB 7.2 does not support this.
        x = gdb.find_pc_line(inst_ptr)
        if x.symtab is not None:
            file_pos = '{0}:{1}'.format(x.symtab.filename, x.line)
        else:
            file_pos = '(Unknown)'
    f = gdb.execute('info symbol 0x{0:x}'.format(inst_ptr),
                    to_string=True).strip()
    if ' in section' in f:
        f = f.split(' in section')[0]
    return '0x{0:016x} {1} [{2}]'.format(inst_ptr, f, file_pos)


def extract_call_stack(inferior, rip, rbp):
    """Unwind call-stack."""
    yield rip  # Hmmm...
    try:
        while True:
            ip = struct.unpack('Q', inferior.read_memory(rbp + 8, 8))[0]
            if ip == 0:
                return
            yield ip
            rbp = struct.unpack('Q', inferior.read_memory(rbp, 8))[0]
    except GdbMemoryError:
        yield INVALID_INSTRUCTION_POINTER


def describe_call_stack(stack):
    """Dump call-stack as string."""
    result = []
    frame_no = 0
    for ip in stack:
        if frame_no >= MAX_CALL_STACK_DEPTH:
            result.append('(More frames are not shown ...)\n')
            break

        result.append('#{0} {1}'.format(frame_no,
                                        describe_instruction_pointer(ip)))
        frame_no += 1

    # @sa: `try_extract_fiber`, if the fiber is being swapped in / out, its call
    # stack can be wrong.
    if 'jump_context' in result[0]:
        result.append('Warning: The fiber is being swapped in / out, '
                      'the call stack shown here can be wrong.')
    return result


class InfoFibersCommand(gdb.Command):
    """This class implements `info fiber` command."""

    def __init__(self):
        super(InfoFibersCommand, self).__init__('info fibers',
                                                gdb.COMMAND_STATUS)

    def invoke(self, argument, from_tty):
        """Implements `info fibers`."""
        inferior = _get_inferior()
        fibers = get_fibers(inferior)

        print('{0:>4}\t{1}'.format('Id', 'Frame'))
        for fiber in fibers:
            print('{0:>4}\t{1}'.format(fiber.id,
                                       describe_instruction_pointer(fiber.rip)))


# TODO(luobogao): Implement `fiber apply all bt` instead.
class ListFibersCommand(gdb.Command):
    """This class implements `list-fibers` command."""

    def __init__(self):
        super(ListFibersCommand, self).__init__('list-fibers', COMMAND_USER)

    def invoke(self, argument, from_tty):
        """Implements `list-fibers`."""
        inferior = _get_inferior()
        fibers = get_fibers(inferior)
        found = 0

        for fiber in fibers:
            found += 1
            print('Fiber #{0}:'.format(fiber.id))
            print('RIP 0x{:016x} RBP 0x{:016x} RSP 0x{:016x}'.format(
                fiber.rip, fiber.rbp, fiber.rsp))
            print('\n'.join(
                describe_call_stack(
                    extract_call_stack(inferior, fiber.rip, fiber.rbp))))
            print('')

        print('Found {0} fiber(s) in total.'.format(found))


class ListFibersCompactCommand(gdb.Command):
    """Same as `list-fibers` except that this command collapse identical
    callstacks.
    """

    def __init__(self):
        super(ListFibersCompactCommand, self).__init__('list-fibers-compact',
                                                       COMMAND_USER)

    def invoke(self, argument, from_tty):
        """Implements `list-fibers-compact`."""
        inferior = _get_inferior()
        fibers = get_fibers(inferior)
        found = 0
        stack_to_fibers = {}

        for fiber in fibers:
            found += 1
            stack = list(extract_call_stack(inferior, fiber.rip, fiber.rbp))
            key = repr(stack)
            if key in stack_to_fibers:
                stack_to_fibers[key][1].append(fiber)
            else:
                stack_to_fibers[key] = [stack, [fiber]]

        for key in stack_to_fibers:
            stack, fibers = stack_to_fibers[key]
            print('Fiber {0}:'.format(', '.join(
                ['#' + str(x.id) for x in fibers])))
            print('\n'.join(describe_call_stack(stack)))
            print('')

        print('Found {0} fiber(s) in total.'.format(found))


InfoFibersCommand()
ListFibersCommand()
ListFibersCompactCommand()
