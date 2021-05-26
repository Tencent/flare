cc_library(
  name = 'rapidxml',
  hdrs = 'rapidxml.h',
  deps = [
    '//thirdparty/rapidxml/1.13:rapidxml',
  ],
  visibility = 'PUBLIC',
)

cc_binary(
  name = 'rapidxml_test',
  srcs = [
	  'rapidxml_test.cpp',
  ],
  deps = ':rapidxml'
)
