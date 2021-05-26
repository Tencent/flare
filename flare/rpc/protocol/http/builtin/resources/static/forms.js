// Copyright 2010 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// From google/protobuf/descriptor.proto
var BASE_PATH;
var QUERY_STR = "";

(function init() {
    var parts = window.location.href.split("?");
    BASE_PATH = parts[0];
    if (parts.length == 2) {
        QUERY_STR = '?' + parts[1];
    }
}());

var FIELD_LABEL = {
    OPTIONAL: 1,
    REQUIRED: 2,
    REPEATED: 3
};

var FIELD_LABEL_STRING = {
    OPTIONAL: "LABEL_OPTIONAL",
    REQUIRED: "LABEL_REQUIRED",
    REPEATED: "LABEL_REPEATED"
};

var FIELD_TYPE = {
    DOUBLE: 1,
    FLOAT: 2,
    INT64: 3,
    UINT64: 4,
    INT32: 5,
    FIXED64: 6,
    FIXED32: 7,
    BOOL: 8,
    STRING: 9,
    GROUP: 10,
    MESSAGE: 11,
    BYTES: 12,
    UINT32: 13,
    ENUM: 14,
    SFIXED32: 15,
    SFIXED64: 16,
    SINT32: 17,
    SINT64: 18
};

var FIELD_TYPE_STRING = {
    DOUBLE: "TYPE_DOUBLE",
    FLOAT: "TYPE_FLOAT",
    INT64: "TYPE_INT64",
    UINT64: "TYPE_UINT64",
    INT32: "TYPE_INT32",
    FIXED64: "TYPE_FIXED64",
    FIXED32: "TYPE_FIXED32",
    BOOL: "TYPE_BOOL",
    STRING: "TYPE_STRING",
    GROUP: "TYPE_GROUP",
    MESSAGE: "TYPE_MESSAGE",
    BYTES: "TYPE_BYTES",
    UINT32: "TYPE_UINT32",
    ENUM: "TYPE_ENUM",
    SFIXED32: "TYPE_SFIXED32",
    SFIXED64: "TYPE_SFIXED64",
    SINT32: "TYPE_SINT32",
    SINT64: "TYPE_SINT64"
};

var FIELD_TYPE_NAME = function () {
    var result = {};
    for (var key in FIELD_TYPE) {
        result[FIELD_TYPE[key]] = key.toLowerCase();
    }
    for (var key in FIELD_TYPE_STRING) {
        result[FIELD_TYPE_STRING[key]] = key.toLowerCase();
    }
    return result;
}();


var LIMITS = {
    INT32_MIN: -2147483648,
    INT32_MAX: 2147483647,
    UINT32_MAX: 4294967295,
    FLOAT_MIN: -3.40282346638528859812e+38,
    FLOAT_MAX: 3.40282346638528859812e+38,
    INT64_MIN: '-9223372036854775808',
    INT64_MAX: '9223372036854775807',
    UINT64_MAX: '18446744073709551615'
};


function parseInterger32(input, reg, min, max) {
    var v = $.trim(input.val());
    if (v.length == 0)
        throw 'is empty';

    v = v.toLowerCase();
    if (!reg.test(v)) {
        throw 'is invalid';
    }

    if (!isFinite(v))
        throw 'out of range';

    var n = Number(v);
    if (n >= min && n <= max)
        return n;
    else
        throw 'out of range';
}

function parseInterger64(input, reg, max, hex_max, min) {
    var v = $.trim(input.val());
    if (v.length == 0)
        throw 'is empty';

    v = v.toLowerCase();
    if (!reg.test(v)) {
        throw 'is invalid';
    }

    if (!isFinite(v))
        throw 'out of range';

    if (v.charAt(0) == '-') {
        if (v.length > min.length || (v.length == min.length && v > min))
            throw 'out of range';
    } else {
        if (v.charAt(0) == '0' && v.charAt(1) == 'x') {
            if (v.length > 18 || (v.length == 18 && v > hex_max))
                throw 'out of range';
        } else {
            if (v.length > max.length || (v.length == max.length && v > max))
                throw 'out of range';
        }
    }

    // Javascript不支持64长整数，转成Number会有精度损失
    // 因此，在json中通过字符串格式传输到64长整数以保证精度
    return v;
}

var TypeParser = [];

TypeParser[FIELD_TYPE.INT32] = function (input) {
    return parseInterger32(input,
        /^(?:0x[0-9a-f]+|-?\d+)$/,
        LIMITS.INT32_MIN,
        LIMITS.INT32_MAX);
};
TypeParser[FIELD_TYPE.SINT32] = TypeParser[FIELD_TYPE.INT32];
TypeParser[FIELD_TYPE.SFIXED32] = TypeParser[FIELD_TYPE.INT32];

TypeParser[FIELD_TYPE.INT64] = function (input) {
    return parseInterger64(input,
        /^(?:0x[0-9a-f]+|-?\d+)$/,
        LIMITS.INT64_MAX,
        '0x7fffffffffffffff',
        LIMITS.INT64_MIN);
};
TypeParser[FIELD_TYPE.SINT64] = TypeParser[FIELD_TYPE.INT64];
TypeParser[FIELD_TYPE.SFIXED64] = TypeParser[FIELD_TYPE.INT64];

TypeParser[FIELD_TYPE.UINT32] = function (input) {
    return parseInterger32(input,
        /^(?:0x[0-9a-f]+|\d+)$/,
        0,
        LIMITS.UINT32_MAX);
};
TypeParser[FIELD_TYPE.FIXED32] = TypeParser[FIELD_TYPE.UINT32];

TypeParser[FIELD_TYPE.UINT64] = function (input) {
    return parseInterger64(input,
        /^(?:0x[0-9a-f]+|\d+)$/,
        LIMITS.UINT64_MAX,
        '0xffffffffffffffff');
};
TypeParser[FIELD_TYPE.FIXED64] = TypeParser[FIELD_TYPE.UINT64];

TypeParser[FIELD_TYPE.DOUBLE] = function (input) {
    var v = $.trim(input.val());
    if (v.length == 0)
        throw 'is empty';

    if (!isFinite(v))
        throw 'out of range';

    return Number(v);
};

TypeParser[FIELD_TYPE.FLOAT] = function (input) {
    var n = TypeParser[FIELD_TYPE.DOUBLE](input);
    if (n >= LIMITS.FLOAT_MIN && n <= LIMITS.FLOAT_MAX)
        return n;
    else
        throw 'out of range';
};

TypeParser[FIELD_TYPE.BOOL] = function (input) {
    return input[0].checked;
};

TypeParser[FIELD_TYPE.ENUM] = function (input) {
    var n = parseInt(input.val());
    if (isNaN(n))
        throw 'is not selected.';
    return n;
};

TypeParser[FIELD_TYPE.STRING] = function (input) {
    // return encodeURIComponent(input.val());
    return input.val();
};

TypeParser[FIELD_TYPE.MESSAGE] = null;

TypeParser[FIELD_TYPE.BYTES] = function (input) {
    // TODO(ericliu) 非ASCII码字符以percent编码形式表示
    return input.val();
};

TypeParser[FIELD_TYPE_STRING.INT32] = function (input) {
    return parseInterger32(input,
        /^(?:0x[0-9a-f]+|-?\d+)$/,
        LIMITS.INT32_MIN,
        LIMITS.INT32_MAX);
};
TypeParser[FIELD_TYPE_STRING.SINT32] = TypeParser[FIELD_TYPE_STRING.INT32];
TypeParser[FIELD_TYPE_STRING.SFIXED32] = TypeParser[FIELD_TYPE_STRING.INT32];

TypeParser[FIELD_TYPE.INT64] = function (input) {
    return parseInterger64(input,
        /^(?:0x[0-9a-f]+|-?\d+)$/,
        LIMITS.INT64_MAX,
        '0x7fffffffffffffff',
        LIMITS.INT64_MIN);
};
TypeParser[FIELD_TYPE_STRING.SINT64] = TypeParser[FIELD_TYPE_STRING.INT64];
TypeParser[FIELD_TYPE_STRING.SFIXED64] = TypeParser[FIELD_TYPE_STRING.INT64];

TypeParser[FIELD_TYPE_STRING.UINT32] = function (input) {
    return parseInterger32(input,
        /^(?:0x[0-9a-f]+|\d+)$/,
        0,
        LIMITS.UINT32_MAX);
};
TypeParser[FIELD_TYPE_STRING.FIXED32] = TypeParser[FIELD_TYPE_STRING.UINT32];

TypeParser[FIELD_TYPE_STRING.UINT64] = function (input) {
    return parseInterger64(input,
        /^(?:0x[0-9a-f]+|\d+)$/,
        LIMITS.UINT64_MAX,
        '0xffffffffffffffff');
};
TypeParser[FIELD_TYPE_STRING.FIXED64] = TypeParser[FIELD_TYPE_STRING.UINT64];

TypeParser[FIELD_TYPE_STRING.DOUBLE] = function (input) {
    var v = $.trim(input.val());
    if (v.length == 0)
        throw 'is empty';

    if (!isFinite(v))
        throw 'out of range';

    return Number(v);
};

TypeParser[FIELD_TYPE_STRING.FLOAT] = function (input) {
    var n = TypeParser[FIELD_TYPE_STRING.DOUBLE](input);
    if (n >= LIMITS.FLOAT_MIN && n <= LIMITS.FLOAT_MAX)
        return n;
    else
        throw 'out of range';
};

TypeParser[FIELD_TYPE_STRING.BOOL] = function (input) {
    return input[0].checked;
};

TypeParser[FIELD_TYPE_STRING.ENUM] = function (input) {
    var n = parseInt(input.val());
    if (isNaN(n))
        throw 'is not selected.';
    return n;
};

TypeParser[FIELD_TYPE_STRING.STRING] = function (input) {
    // return encodeURIComponent(input.val());
    return input.val();
};

TypeParser[FIELD_TYPE_STRING.MESSAGE] = null;

TypeParser[FIELD_TYPE_STRING.BYTES] = function (input) {
    // TODO(ericliu) 非ASCII码字符以percent编码形式表示
    return input.val();
};

// Convert full qualified name to full name,
// Protobuf *Proto will all a prefix '.' for full qualified typename, remove it
function toUnqualified(typeName) {
    if (typeName.indexOf('.') === 0) {
        return typeName.substr(1);
    }
    return typeName;
}

/**
 * Data structure used to represent a form to data element.
 * @param {Object} field Field descriptor that form element represents.
 * @param {Object} container Element that contains field.
 * @return {FormElement} New object representing a form element.  Element
 *     starts enabled.
 * @constructor
 */
function FormElement(field, container) {
    this.field = field;
    this.container = container;
    this.enabled = true;
}


/**
 * Display error message in error panel.
 * @param {string} message Message to display in panel.
 */
function error(message) {
    $('<div>').appendTo($('#error-messages')).html(htmlEscape(message));
}


/**
 * Display request errors in error panel.
 * @param {object} XMLHttpRequest object.
 */
function handleRequestError(response) {
    var contentType = response.getResponseHeader('content-type');
    if (contentType == 'application/json') {
        var response_error = $.parseJSON(response.responseText);
        var error_message = response_error.error_message;
        if (error.state == 'APPLICATION_ERROR' && error.error_name) {
            error_message = error_message + ' (' + error.error_name + ')';
        }
    } else {
        error_message = response.status + ': ' + response.statusText;
        if (response.responseText.length > 0)
            error_message = error_message + ', ' + response.responseText;
    }

    error(error_message);

    send_button.disabled = false;
}


/**
 * Send JSON RPC to remote method.
 * @param {string} service_name service full name.
 * @param {string} method Name of method to invoke.
 * @param {Object} request Message to send as request.
 * @param {function} on_success Function to call upon successful request.
 */
function sendRequest(methodFullName, inputContentType, responseFormat,
    request, onSuccess) {
    $.ajax({
        url: methodFullName,
        type: 'POST',
        beforeSend: function (header) {
            header.setRequestHeader('Accept', responseFormat);
            header.setRequestHeader('Rpc-SeqNo', '1');
        },
        contentType: inputContentType,
        data: request,
        // 让jQuery根据http response的MIME自动识别应答格式
        // dataType: 'json',
        success: onSuccess,
        error: handleRequestError
    });
}

/**
 * Send JSON RPC to remote method.
 * @param {string} service_name service full name.
 * @param {string} method Name of method to invoke.
 * @param {Object} request Message to send as request.
 * @param {function} on_success Function to call upon successful request.
 */
function sendGetRequest(url, onSuccess) {
    $.ajax({
        url: url,
        type: 'GET',
        // 让jQuery根据http response的MIME自动识别应答格式
        // dataType: 'json',
        success: onSuccess,
        error: handleRequestError
    });
}



/**
 * Create callback that enables and disables field element when associated
 * checkbox is clicked.
 * @param {Element} checkbox Checkbox that will be clicked.
 * @param {FormElement} form Form element that will be toggled for editing.
 * @return Callback that is invoked every time checkbox is clicked.
 */
function toggleInput(checkbox, form) {
    return function () {
        var checked = checkbox.checked;
        if (checked) {
            if (!form.input)
                buildIndividualForm(form);
            form.enabled = true;
        } else {
            form.enabled = false;
        }
    };
}

/**
 * Set default value to a form element.
 * @param {FormElement} form Form to build element for.
 */
function setDefautValue(form) {
    try {
        form.input.val(decodeURIComponent(form.field.default_value || ''));
    } catch (ex) {
        form.input.val(form.field.default_value || '');
    }
}

/**
 * Build an enum field.
 * @param {FormElement} form Form to build element for.
 */
function buildEnumField(form) {
    form.descriptor = enumDescriptors[toUnqualified(form.field.type_name)];
    form.input = $('<select>').appendTo(form.display);

    $('<option>').appendTo(form.input).attr('value', '').text('Select enum');
    $.each(form.descriptor.value, function (index, enumValue) {
        option = $('<option>');
        option.appendTo(form.input).attr('value', enumValue.number).text(enumValue.name);
        if (enumValue.name == form.field.default_value) {
            option.attr('selected', 1);
        }
    });
}


/**
 * Build nested message field.
 * @param {FormElement} form Form to build element for.
 */
function buildMessageField(form) {
    form.table = $('<table border="1">').appendTo(form.display);
    // A reference will pass to the click callback function if without `var'
    var descriptor = messageDescriptors[toUnqualified(form.field.type_name)];
    if (form.field.label == FIELD_LABEL.REQUIRED ||
        form.field.label == FIELD_LABEL_STRING.REQUIRED ||
        form.field.label == FIELD_LABEL.REPEATED ||
        form.field.label == FIELD_LABEL_STRING.REPEATED) {
        buildMessageForm(form, descriptor);
    } else {
        // To avoid infinity recursive message, for optional message field,
        // delay render the form.
        var expand_button = $('<button>').text('...').appendTo(form.table);
        expand_button.attr('title', 'Hint: expand sub message field.');
        expand_button.click(function () {
            buildMessageForm(form, descriptor);
            expand_button.prop('disabled', true);
            expand_button.hide();
            form.checkbox.click().change();
        });
    }
    // Input is not used for message field, assign a value to avoid build it again
    form.input = $('<input>').hide();
}


/**
 * Build boolean field.
 * @param {FormElement} form Form to build element for.
 */
function buildBooleanField(form) {
    form.input = $('<input type="checkbox">');
    // NOTE: typeof default_value is a string, so Boolean('false') get true!
    form.input.attr('checked', form.field.default_value === 'true');
}


/**
 * Build string field.
 * @param {FormElement} form Form to build element for.
 */
function buildStringField(form) {
    // Using textarea for input multiple line string.
    form.input = $('<textarea>');
    form.input.attr('title', 'Hint: value will always be transfered as UTF-8 encoding.');
    setDefautValue(form);
}


/**
 * Build text field.
 * @param {FormElement} form Form to build element for.
 */
function buildTextField(form) {
    form.input = $('<input type="text">');
    setDefautValue(form);

    switch (form.field.type) {
        case FIELD_TYPE.INT32:
        case FIELD_TYPE_STRING.INT32:
        case FIELD_TYPE.INT64:
        case FIELD_TYPE_STRING.INT64:
        case FIELD_TYPE.UINT32:
        case FIELD_TYPE_STRING.UINT32:
        case FIELD_TYPE.UINT64:
        case FIELD_TYPE_STRING.UINT64:
        case FIELD_TYPE.SINT32:
        case FIELD_TYPE_STRING.SINT32:
        case FIELD_TYPE.SINT64:
        case FIELD_TYPE_STRING.SINT64:
        case FIELD_TYPE.FIXED32:
        case FIELD_TYPE_STRING.FIXED32:
        case FIELD_TYPE.FIXED64:
        case FIELD_TYPE_STRING.FIXED64:
        case FIELD_TYPE.SFIXED32:
        case FIELD_TYPE_STRING.SFIXED32:
        case FIELD_TYPE.SFIXED64:
        case FIELD_TYPE_STRING.SFIXED64:
            form.input.attr('title', 'Hint: you can also use "0x" prefix for hex value.');
            break;
        case FIELD_TYPE.FLOAT:
        case FIELD_TYPE_STRING.FLOAT:
        case FIELD_TYPE.DOUBLE:
        case FIELD_TYPE_STRING.DOUBLE:
            form.input.attr('title', 'Hint: you can also use scientific notation.');
            break;
        case FIELD_TYPE.BYTES:
        case FIELD_TYPE_STRING.BYTES:
            form.input.attr('title', 'Hint: you should use %XX encoding for non-ASCII bytes.');
            break;
    }
}

function inputChanged(form) {
    return function () {
        if ((form.field.type == FIELD_TYPE.STRING ||
            form.field.type == FIELD_TYPE_STRING.STRING ||
            form.field.type == FIELD_TYPE.BYTES ||
            form.field.type == FIELD_TYPE_STRING.BYTES) &&
            form.input.val().length == 0) {
            return;
        }

        form.checkbox.attr("checked", form.input.val().length > 0);
        form.checkbox.change();
    }
}

/**
 * Build individual input element.
 * @param {FormElement} form Form to build element for.
 */
function buildIndividualForm(form) {
    if (form.field.type == FIELD_TYPE.ENUM || form.field.type == FIELD_TYPE_STRING.ENUM) {
        buildEnumField(form);
    } else if (form.field.type == FIELD_TYPE.MESSAGE || form.field.type == FIELD_TYPE_STRING.MESSAGE) {
        buildMessageField(form);
    } else if (form.field.type == FIELD_TYPE.BOOL || form.field.type == FIELD_TYPE_STRING.BOOL) {
        buildBooleanField(form);
    } else if (form.field.type == FIELD_TYPE.STRING || form.field.type == FIELD_TYPE_STRING.STRING) {
        buildStringField(form);
    } else {
        buildTextField(form);
    }

    if (((isNaN(form.field.type) && form.field.type != FIELD_TYPE.MESSAGE) ||
        (!isNaN(form.field.type) && form.field.type != FIELD_TYPE_STRING.MESSAGE)) && form.checkbox) {
        // 同时处理keyup和chanage事件是为了解决浏览器的兼容性
        form.input.keyup(inputChanged(form));
        form.input.change(inputChanged(form));
    }
    form.display.append(form.input);
}


/**
 * Deletion of repeated items.  Needs to delete from DOM and
 * also delete from form model.
 * @param {FormElement} form Repeated form element to delete item for.
 */
function delRepeatedFieldItem(form) {
    form.container.remove();
    var index = $.inArray(form, form.parent.fields);
    form.parent.fields.splice(index, 1);
}


/**
 * Add repeated field.  This function is called when an item is added
 * @param {FormElement} form Repeated form element to create item for.
 */
function addRepeatedFieldItem(form) {
    var row = $('<tr>').appendTo(form.display);
    subForm = new FormElement(form.field, row);
    subForm.parent = form;
    form.fields.push(subForm);
    buildFieldForm(subForm, false);
}


/**
 * Build repeated field.  Contains a button that can be used for adding new
 * items.
 * @param {FormElement} form Form to build element for.
 */
function buildRepeatedForm(form) {
    form.fields = [];
    form.display = $('<table border="1" width="100%">').appendTo(form.container);
    var header_row = $('<tr>').appendTo(form.display);
    var header = $('<td colspan="3">').appendTo(header_row);
    var add_button = $('<button>').text('+').appendTo(header);
    add_button.attr('title', 'Hint: add a new repeated item.');
    add_button.click(function () {
        addRepeatedFieldItem(form);
    });
}


var objectId = 0;

function generateObjectId() {
    return objectId++;
}

/**
 * Build a form field.  Populates form content with values required by
 * all fields.
 * @param {FormElement} form Repeated form element to create item for.
 * @param allowRepeated {Boolean} Allow display of repeated field.  If set to
 *     to true, will treat repeated fields as individual items of a repeated
 *     field and render it as an individual field.
 */
function buildFieldForm(form, allowRepeated) {
    // All form fields are added to a row of a table.
    var inputData = $('<td>');

    // Set name.
    if (allowRepeated) {
        var nameData = $('<td>');
        var full_nama;
        if (form.field.prefixName) {
            full_name = form.field.prefixName + '.' + form.field.name;
        } else {
            full_name = form.field.name;
        }
        if (comments[full_name]) {
            nameData.attr('title', htmlEscape(comments[full_name]));
        }
        var field_type_name;
        if (form.field.type == FIELD_TYPE.MESSAGE || form.field.type == FIELD_TYPE_STRING.MESSAGE ||
            form.field.type == FIELD_TYPE.ENUM || form.field.type == FIELD_TYPE_STRING.ENUM) {
            field_type_name = toUnqualified(form.field.type_name);
        } else {
            field_type_name = FIELD_TYPE_NAME[form.field.type];
        }
        var html = ' <b>' + htmlEscape(form.field.name) + '</b>';
        if (form.field.label == FIELD_LABEL.REQUIRED ||
            form.field.label == FIELD_LABEL_STRING.REQUIRED) {
            html += '<font color="red">*</font>';
        }
        html += ' :' + htmlEscape(field_type_name);
        if (form.field.label == FIELD_LABEL.REPEATED ||
            form.field.label == FIELD_LABEL_STRING.REPEATED) {
            html += '[]';
        }
        nameData.html(html);
        form.container.append(nameData);
    }

    // Set input.
    form.repeated = (form.field.label == FIELD_LABEL.REPEATED ||
        form.field.label == FIELD_LABEL_STRING.REPEATED);
    if (allowRepeated && form.repeated) {
        inputData.attr('colspan', '2');
        buildRepeatedForm(form);
    } else {
        if (!allowRepeated) {
            inputData.attr('colspan', '2');
        }

        form.display = $('<div>');

        var controlData = $('<td>');
        if (((isNaN(form.field.type) && form.field.label != FIELD_LABEL.REQUIRED) ||
            (!isNaN(form.field.type) && form.field.label != FIELD_LABEL_STRING.REQUIRED)) && allowRepeated) {
            form.enabled = false;
            var checkbox_id = 'checkbox-' + generateObjectId();
            var checkbox = $('<input id="' + checkbox_id + '" type="checkbox">').appendTo(controlData);
            checkbox.attr('title', 'Hint: check this checkbox to enable this optional field.');
            checkbox.change(toggleInput(checkbox[0], form));
            form.checkbox = checkbox;
        } else {
            if (form.repeated) {
                var del_button = $('<button>').text('-').appendTo(controlData);
                del_button.attr('title', 'Hint: delete the item in this row.');
                del_button.click(function () {
                    delRepeatedFieldItem(form);
                });
            }
        }

        buildIndividualForm(form);
        form.container.append(controlData);
    }

    inputData.append(form.display);
    form.container.append(inputData);
}


/**
 * Top level function for building an entire message form.  Called once at form
 * creation and may be called again for nested message fields.  Constructs a
 * a table and builds a row for each sub-field.
 * @params {FormElement} form Form to build message form for.
 */
function buildMessageForm(form, messageType) {
    form.fields = [];
    form.descriptor = messageType;
    if (messageType.field) {
        $.each(messageType.field, function (index, field) {
            if (messageType.prefixName) {
                field.prefixName = messageType.prefixName;
            }
            var row = $('<tr>').appendTo(form.table);
            var fieldForm = new FormElement(field, row);
            fieldForm.parent = form;
            buildFieldForm(fieldForm, true);
            form.fields.push(fieldForm);
        });
    }
}


/**
 * HTML Escape a string
 */
function htmlEscape(value) {
    if (typeof (value) == "string") {
        return value
            .replace(/&/g, '&amp;')
            .replace(/>/g, '&gt;')
            .replace(/</g, '&lt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;')
            .replace(/ /g, '&nbsp;');
    } else {
        return value;
    }
}


/**
 * JSON formatted in HTML for display to users.  This method recursively calls
 * itself to render sub-JSON objects.
 * @param {Object} value JSON object to format for display.
 * @param {Integer} indent Indentation level for object being displayed.
 * @return {string} Formatted JSON object.
 */
function formatJSON(value, indent) {
    var indentation = '';
    var indentation_back = '';
    for (var index = 0; index < indent; ++index) {
        indentation = indentation + '&nbsp;&nbsp;&nbsp;&nbsp;';
        if (index == indent - 2)
            indentation_back = indentation;
    }
    var type = typeof (value);

    var result = '';

    if (type == 'object') {
        if (value.constructor === Array) {
            result += '[<br>';
            $.each(value, function (index, item) {
                result += indentation + formatJSON(item, indent + 1) + ',<br>';
            });
            result += indentation_back + ']';
        } else {
            result += '{<br>';
            $.each(value, function (name, item) {
                result += (indentation + htmlEscape(name) + ': ' +
                    formatJSON(item, indent + 1) + ',<br>');
            });
            result += indentation_back + '}';
        }
    } else {
        try {
            result += htmlEscape(decodeURIComponent(value));
        } catch (ex) {
            result += htmlEscape(value);
        }
    }
    return result;
}


/**
 * Construct array from repeated form element.
 * @param {FormElement} form Form element to build array from.
 * @return {Array} Array of repeated elements read from input form.
 */
function fromRepeatedForm(form) {
    var values = [];
    $.each(form.fields, function (index, subForm) {
        values.push(fromIndividualForm(subForm));
    });
    return values;
}

function getFieldFullname(form) {
    full_name = form.field.name;
    while (typeof form.parent !== "undefined" && form.parent.field != null) {
        full_name = form.parent.field.name + '.' + full_name;
        form = form.parent;
    }
    return full_name;
}

var first_failed_input;
var failed_messages;

/**
 * Construct value from individual form element.
 * @param {FormElement} form Form element to get value from.
 * @return {string, Float, Integer, Boolean, object} Value extracted from
 *     individual field.  The type depends on the field variant.
 */
function fromIndividualForm(form) {
    if (form.field.type == FIELD_TYPE.MESSAGE ||
        form.field.type == FIELD_TYPE_STRING.MESSAGE)
        return fromMessageForm(form);
    try {
        return TypeParser[form.field.type](form.input);
    } catch (ex) {
        failed_messages.push(getFieldFullname(form) + ' ' + ex + '.');
        if (!first_failed_input)
            first_failed_input = form.input;
        return null;
    }
}


/**
 * Extract entire message from a complete form.
 * @param {FormElement} form Form to extract message from.
 * @return {Object} Fully populated message object ready to transmit
 *     as JSON message.
 */
function fromMessageForm(form) {
    var message = {};
    $.each(form.fields, function (index, subForm) {
        if (subForm.enabled) {
            var subMessage = undefined;
            if (subForm.field.label == FIELD_LABEL.REPEATED ||
                subForm.field.label == FIELD_LABEL_STRING.REPEATED) {
                subMessage = fromRepeatedForm(subForm);
            } else {
                subMessage = fromIndividualForm(subForm);
            }
            message[subForm.field.name] = subMessage;
        }
    });

    return message;
}

var g_methodFullName;
var g_modeForm;
var g_activeMode;


/**
 * Send form as an RPC.  Extracts message from root form and transmits to
 * originating poppy server.  Response is formatted as JSON and displayed
 * to user.
 */

function sendForm() {
    send_button = this;
    send_button.disabled = true;

    $('#error-messages').empty();
    $('#form-response').empty();

    var request, content_type;
    if (g_activeMode.id == g_modeForm.id) {
        first_failed_input = null;
        failed_messages = [];
        message = fromMessageForm(root_form);
        if (failed_messages.length > 0) {
            error("" + failed_messages.join('\n'));
            var html = '<h3>Input Error:</h3>';
            $.each(value, function (index, item) {
                html += '<br/>' + htmlEscape(item);
            });
            $('<div>').appendTo($('#error-messages')).html(html);
            first_failed_input.focus();
            first_failed_input.select();
            send_button.disabled = false;
            return;
        }
        request = $.toJSON(message);
        content_type = 'application/json';
    } else {
        request = root_form.request_editor.doc.getValue();
        content_type = root_form.request_format.val();
    }

    var req_enc = root_form.request_encoding.val();
    var resp_enc = root_form.response_encoding.val();
    var qs = '';
    if (req_enc != '') qs += '&ie=' + req_enc;
    if (resp_enc == '') {
        qs += '&oe=' + req_enc;
    } else {
        qs += '&oe=' + resp_enc;
    }
    if (root_form.use_beautiful.attr("checked")) {
        qs += '&b=1';
    }
    if (qs != "") {
        qs = '?' + qs;
    }

    sendRequest(BASE_PATH + '/../../../rpc/' + encodeURIComponent(g_methodFullName), // + qs + QUERY_STR
        content_type, 'text/x-protobuf', request,
        function (response) {
            send_button.disabled = false;
            $('#form-response').html('<h3>Response:</h3>' + htmlEscape(JSON.stringify(response)));
            if (request.length > 0)
                hideForm();
        });
}


/**
 * Reset form to original state.  Deletes existing form and rebuilds a new
 * one from scratch.
 * @param {Object} response ajax response.
 */
function resetForm() {
    var panel = $('#form-panel');

    panel.empty();

    function formGenerationError(message) {
        error(message);
        panel.html('<div class="error-message">' +
            'There was an error generating the service form' +
            '</div>');
    }

    requestType = messageDescriptors[requestTypeName];
    if (!requestType) {
        formGenerationError('No such message-type: ' + requestTypeName);
        return;
    }
    requestType.prefixName = requestTypeName;

    if (requestType.field)
        requestType.field.sort(function (field1, field2) {
            return field1.number - field2.number;
        });

    $('<div>').appendTo(panel).html('<br/>Request message type is '
        + htmlEscape(requestTypeName) + '<br/><br/>');

    root_form = new FormElement(null, null);
    var mode_id = g_activeMode.id;
    if (mode_id == g_modeForm.id) {
        var root = $('<table border="1">').appendTo(panel);
        root_form.table = root;
        buildMessageForm(root_form, requestType);
    } else {
        $('<label>').appendTo(panel).text('request format ');
        var content_type_protobuf = 'text/x-protobuf';
        var content_type_json = 'application/json';
        var message_example = "Example: \n" +
            "message EchoRequest {\n" +
            "  optional string request = 1;\n" +
            "  optional int32 response_length = 2;\n}\n";
        root_form.request_example = {};
        root_form.request_example[content_type_protobuf] = message_example +
            "You should input as below: \n" +
            "request: 'hello'\nresponse_length: 5\n";
        root_form.request_example[content_type_json] = message_example +
            "You should input as below: \n" +
            '{\n  "request": "hello",\n  "response_length": 5\n}';
        var request_fmt = $('<select>').appendTo(panel).change(function () {
            root_form.request_editor.getWrapperElement().title =
                root_form.request_example[$(this).val()];
        });
        root_form.request_format = request_fmt;
        var option = $('<option>');
        option.appendTo(request_fmt).attr(
            'value', content_type_protobuf).text('Protobuf');
        option.attr('selected', 1);
        $('<option>').appendTo(request_fmt).attr(
            'value', content_type_json).text('JSON');
        $('<br>').appendTo(panel);
        $('<br>').appendTo(panel);
        // remember the textarea's id to get value in function sendForm
        $('<textarea>').appendTo(panel).attr("id", "text_input");
        var myCodeMirror = CodeMirror.fromTextArea(
            document.getElementById("text_input"), { autofocus: true });

        $(myCodeMirror.getWrapperElement()).css(
            { "width": "35%", "height": "50%", "background": "Beige" }).attr(
                "title", root_form.request_example[content_type_protobuf]);
        root_form.request_editor = myCodeMirror;
    }
    $('<br>').appendTo(panel);
    buildEncodingForm(panel);
    $('<br>').appendTo(panel);
    $('<br>').appendTo(panel);
    $('<button>').appendTo(panel).text('Send Request').click(sendForm);
    if (requestType.field || mode_id != g_modeForm.id) {
        panel.append('&nbsp;&nbsp;&nbsp;&nbsp;');
        $('<button>').appendTo(panel).text('Reset').click(resetForm);
    } else {
        $('#form-expander').hide();
    }
}

/**
 * Build encoding selection.
 * @param {panel} container for display.
 */
function buildEncodingForm(panel) {
    $('<label>').appendTo(panel).text('Request encoding:');

    var request_enc = $('<select>').appendTo(panel);
    root_form.request_encoding = request_enc;

    var option = $('<option>');
    option.appendTo(request_enc).attr('value', 'utf-8').text('UTF-8');
    option.attr('selected', 1);

    $('<option>').appendTo(request_enc).attr('value', 'gbk').text('GBK');
    $('<option>').appendTo(request_enc).attr('value', 'big5').text('BIG5');

    panel.append('&nbsp;&nbsp;');
    $('<label>').appendTo(panel).text('Response encoding:');

    var response_enc = $('<select>').appendTo(panel);
    root_form.response_encoding = response_enc;

    option = $('<option>');
    option.appendTo(response_enc).attr('value', '').text('Same as request');
    option.attr('selected', 1);

    $('<option>').appendTo(response_enc).attr('value', 'utf-8').text('UTF-8');
    $('<option>').appendTo(response_enc).attr('value', 'gbk').text('GBK');
    $('<option>').appendTo(response_enc).attr('value', 'big5').text('BIG5');

    $('<br>').appendTo(panel);
    $('<br>').appendTo(panel);
    $('<label>').appendTo(panel).text(' ');
    root_form.use_beautiful =
        $('<input type="checkbox">').appendTo(panel);
    root_form.use_beautiful.attr("checked", true);
    $('<label>').appendTo(panel).text('Use beautiful text format for response');
}


var FORM_VISIBILITY = {
    SHOW_FORM: 'Show Form',
    HIDE_FORM: 'Hide Form'
};


/**
 * Hide main RPC form from user.  The information in the form is preserved.
 * Called after RPC to server is completed.
 */
function hideForm() {
    var expander = $('#form-expander');
    var formPanel = $('#form-panel');
    formPanel.hide();
    expander.text(FORM_VISIBILITY.SHOW_FORM);
}


/**
 * Toggle the display of the main RPC form.  Called when form expander button
 * is clicked.
 */
function toggleForm() {
    var expander = $('#form-expander');
    var formPanel = $('#form-panel');
    if (expander.text() == FORM_VISIBILITY.HIDE_FORM) {
        hideForm();
    } else {
        formPanel.show();
        expander.text(FORM_VISIBILITY.HIDE_FORM);
    }
}


/**
 * Create form.
 */
function createForm(methodFullName) {
    g_methodFullName = methodFullName;
    g_modeForm = document.getElementById('input-form');
    g_activeMode = g_modeForm;
    document.getElementById("method-selection").href = '../rpc' + QUERY_STR;
    sendGetRequest(
        BASE_PATH + '/' + '../../../inspect/rpc_reflect/method/' + methodFullName + QUERY_STR,
        function (response) {
            requestTypeName = toUnqualified(response.method.input_type);
            messageDescriptors = {};
            $.each(response.message_type, function (index, message_type) {
                messageDescriptors[message_type.full_name] = message_type.info;
            });
            enumDescriptors = {};
            // 下面的条件判断等价于表达式 (typeof response.enum_types !== "undefined")
            if (response.enum_type) {
                $.each(response.enum_type, function (index, enum_type) {
                    enumDescriptors[enum_type.full_name] = enum_type.info;
                });
            }
            comments = {}
            if (response.comments) {
                $.each(response.comments, function (index, comment) {
                    comments[comment.full_name] = "";
                    if (comment.leading_comments) {
                        comments[comment.full_name] += comment.leading_comments;
                    }
                    if (comment.trailing_comments) {
                        if (comments[comment.full_name].length > 0) {
                            comments[comment.full_name] += "\n";
                        }
                        comments[comment.full_name] += comment.trailing_comments;
                    }
                });
            }
            $(".top-bar a").click(
                function () {
                    if (this.id != g_activeMode.id) {
                        g_activeMode.className = 'input-mode-not-selected';
                        g_activeMode = this;
                        g_activeMode.className = 'input-mode-selected';
                        resetForm();
                    }
                });
            $('#form-expander').click(toggleForm);
            resetForm();
        });
}


/**
 * Display available services and their methods.
 * @param {Array} all service descriptors.
 */
function showServices(services) {
    var methodSelector = $('#method-selector');
    $.each(services, function (index, service) {
        $('<h3>').text(service.full_name).appendTo(methodSelector);
        var ul = $('<ul>').appendTo(methodSelector);
        var block = $('<blockquote>').appendTo(methodSelector);
        $.each(service.info.method, function (index, method) {
            var li = $('<li>').appendTo(ul);
            var url = (BASE_PATH + '/../rpc/' + encodeURIComponent(service.full_name) +
                '.' + encodeURIComponent(method.name) + QUERY_STR);
            var label = method.name;
            $('<a>').attr('href', url).text(label).appendTo(li);
        });
    });
}


/**
 * Load all services from poppy registry service.  When services are
 * loaded, will then show all services and methods from the server.
 */
function loadServices() {
    sendGetRequest(
        BASE_PATH + '/' + '../../inspect/rpc_reflect/services' + QUERY_STR,
        function (response) {
            showServices(response.service);
        });
}
