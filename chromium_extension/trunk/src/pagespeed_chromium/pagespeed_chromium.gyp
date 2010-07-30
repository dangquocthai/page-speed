# Copyright 2010 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'variables': {
    'libpagespeed_root': '<(DEPTH)/third_party/libpagespeed/src',
  },
  # TODO(mdsteele): Add conditions as necessary so that we can build on
  #                 non-x86_64 platforms as well.
  'targets': [
    {
      'target_name': 'pagespeed_x86_64.nexe',
      'type': 'executable',
      'dependencies': [
      ],
      'sources': [
        'npn_bridge.cc',
        'npp_gate.cc',
        'pagespeed_chromium.cc',
        'pagespeed_module.cc',
      ],
      'defines': [
        # TODO(mdsteele): What does this define do?  I think it has something
        #                 to do with NPAPI.  I copied it from the NaCl SDK
        #                 example code.  Do we even need it?
        'XP_UNIX',
      ],
      'ldflags': [
        '-lgoogle_nacl_imc',
        '-lgoogle_nacl_npruntime',
        '-lpthread',
        '-lsrpc',
      ],
    },

    {
      'target_name': 'pagespeed_extension',
      'type': 'none',
      'dependencies': [
        'pagespeed_x86_64.nexe',
      ],
      'actions': [
        {
          'action_name': 'make_dir',
          'inputs': [],
          'outputs': [
            '<(PRODUCT_DIR)/pagespeed',
          ],
          'action': [
            'mkdir', '<@(_outputs)',
          ],
        },
        {
          'action_name': 'copy_files',
          'inputs': [
            '<(PRODUCT_DIR)/pagespeed',
            'extension_files/icon.png',
            'extension_files/manifest.json',
            'extension_files/popup.html',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/pagespeed/icon.png',
            '<(PRODUCT_DIR)/pagespeed/manifest.json',
            '<(PRODUCT_DIR)/pagespeed/popup.html',
          ],
          'action': [
            'cp', '-t', '<@(_inputs)',
          ],
        },
        {
          'action_name': 'copy_nexe',
          'inputs': [
            '<(PRODUCT_DIR)/pagespeed',
            '<(PRODUCT_DIR)/pagespeed_x86_64.nexe',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/pagespeed/pagespeed_x86_64.nexe',
          ],
          'action': [
            'cp', '<(PRODUCT_DIR)/pagespeed_x86_64.nexe',
            '<(PRODUCT_DIR)/pagespeed/',
          ],
        },
      ],
    },
  ],
}
