# for ninja:
# meson setup builddir
# cd builddir ; meson compile

# regenerate configuration (run from builddir/)
# meson setup --reconfigure --wipe

project('ccid', 'c',
  meson_version : '>=0.58.0',
  version : '1.7.0')

# for config.h
conf_data = configuration_data({
  'VERSION' : '"' + meson.project_version() + '"',
  'ENABLE_ZLP' : get_option('zlp'),
  'NO_LOG' : get_option('embedded'),
  'USE_OS_LOG' : get_option('os_log'),
  'USE_COMPOSITE_AS_MULTISLOT' : get_option('composite-as-multislot'),
  })

# global arguments
add_global_arguments('-fvisibility=hidden', language : 'c')

# tests for functions
compiler = meson.get_compiler('c')
if compiler.has_function('strlcpy')
  conf_data.set('HAVE_STRLCPY', true)
endif
if compiler.has_function('strlcat')
  conf_data.set('HAVE_STRLCAT', true)
endif
if compiler.has_function('secure_getenv')
  conf_data.set('HAVE_SECURE_GETENV', true)
endif
if compiler.has_function('pthread_condattr_setclock')
  conf_data.set('HAVE_PTHREAD_CONDATTR_SETCLOCK', true)
endif

# variables
bundle_id = 'ifd-ccid.bundle'
extra_bundle_id = get_option('extra_bundle_id')
if extra_bundle_id != ''
  bundle_id = 'ifd-ccid-' + extra_bundle_id + '.bundle'
else
endif
dyn_lib_ext = '.so'

pcsc_dep = dependency('libpcsclite')
pcsc_cflags = pcsc_dep.partial_dependency(compile_args : true)
libusb_dep = dependency('libusb-1.0')
zlib_dep = dependency('zlib')
threads_dep = dependency('threads')

r = run_command('uname', check: true)
pcsc_arch = r.stdout().strip()
if pcsc_arch == 'Darwin'
  pcsc_arch = 'MacOS'
  dyn_lib_ext = '.dylib'
  libusb_dep = dependency('libusb-1.0', static : true)
endif

# flex generator
gen_flex = generator(find_program('flex'),
  output : '@BASENAME@.c',
  arguments : ['-o', '@OUTPUT@', '--prefix=@BASENAME@', '@INPUT@'])

conf_data.set_quoted('PCSCLITE_HP_DROPDIR', pcsc_dep.get_variable('usbdropdir'))
conf_data.set_quoted('BUNDLE', bundle_id)

# libccid
libccid_src = [
  'src/ccid.c',
  'src/ccid_usb.c',
  'src/commands.c',
  'src/ifdhandler.c',
  'src/simclist.c',
  'src/strlcpy.c',
  'src/sys_unix.c',
  'src/utils.c',
  'src/openct/buffer.c',
  'src/openct/checksum.c',
  'src/openct/proto-t1.c',
  'src/towitoko/atr.c',
  'src/towitoko/pps.c',
  ]
gen_src = gen_flex.process('src/tokenparser.l')
libccid_src += gen_src
if not get_option('pcsclite')
  libccid_src += 'src/debug.c'
endif

library('ccid',
  libccid_src,
  c_args : ['-DSIMCLIST_NO_DUMPRESTORE'],
  override_options : ['b_lundef=false'],
  include_directories : ['src'],
  dependencies : [libusb_dep, pcsc_cflags],
  install : true,
  install_dir : join_paths(pcsc_dep.get_variable('usbdropdir'), bundle_id, 'Contents', pcsc_arch))

if get_option('serial')
# libccidtwin
libccidtwin_src = [
  'src/ccid.c',
  'src/ccid_serial.c',
  'src/commands.c',
  'src/ifdhandler.c',
  'src/simclist.c',
  'src/strlcpy.c',
  'src/sys_unix.c',
  'src/utils.c',
  'src/openct/buffer.c',
  'src/openct/checksum.c',
  'src/openct/proto-t1.c',
  'src/towitoko/atr.c',
  'src/towitoko/pps.c',
  gen_src,
  ]
if not get_option('pcsclite')
  libccidtwin_src += 'src/debug.c'
endif

libccidtwin_dir = join_paths(pcsc_dep.get_variable('usbdropdir'), 'serial')
library('ccidtwin',
  libccidtwin_src,
  c_args : ['-DSIMCLIST_NO_DUMPRESTORE', '-DTWIN_SERIAL'],
  override_options : ['b_lundef=false'],
  include_directories : ['src'],
  dependencies : [pcsc_cflags],
  install : true,
  install_dir : libccidtwin_dir)

# reader.conf
conf_data.set('TARGET', join_paths(libccidtwin_dir, 'libccidtwin' + dyn_lib_ext))
configure_file(output : 'libccidtwin',
  input : 'src/reader.conf.in',
  install_dir : join_paths(get_option('sysconfdir'), 'reader.conf.d'),
  configuration : conf_data)
endif

# parse
parse_src = [
  'src/parse.c',
  'src/debug.c',
  'src/ccid_usb.c',
  'src/sys_unix.c',
  'src/strlcpy.c',
  'src/simclist.c',
  ]
parse_src += gen_src

executable('parse',
  parse_src,
  include_directories : ['src'],
  dependencies : [libusb_dep, pcsc_cflags, zlib_dep, threads_dep],
  )

if get_option('enable-extras')
  # examples scardcontrol
  scardcontrol_src = [
    'examples/scardcontrol.c',
    'examples/PCSCv2part10.c',
    ]
  executable('scardcontrol',
    scardcontrol_src,
    dependencies : [pcsc_dep],
    )

  # contrib Kobil_mIDentity_switch
  executable('Kobil_mIDentity_switch',
    'contrib/Kobil_mIDentity_switch/Kobil_mIDentity_switch.c',
    dependencies : [libusb_dep],
    )

  # contrib RSA_SecurID_getpasswd
  executable('RSA_SecurID_getpasswd',
    'contrib/RSA_SecurID/RSA_SecurID_getpasswd.c',
    dependencies : [pcsc_dep],
    )
endif

# Info.plist
command = [
    find_program('src/create_Info_plist.pl'),
    files('readers/supported_readers.txt'),
    '@INPUT@',
    '--version=' + meson.project_version(),
    '--target=libccid' + dyn_lib_ext
    ]
if not get_option('class')
  command += '--no-class'
endif
if extra_bundle_id != ''
  command += '--extra_bundle_id=.' + extra_bundle_id
endif
custom_target('Info.plist',
  output : 'Info.plist',
  input : 'src/Info.plist.src',
  build_by_default : true,
  capture : true,
  command : command,
  install : true,
  install_dir : join_paths(pcsc_dep.get_variable('usbdropdir'), bundle_id, 'Contents'))

# udev rules
if get_option('udev-rules')
  udev = dependency('udev')
  install_data('src/92_pcscd_ccid.rules',
    install_dir : join_paths(udev.get_variable(pkgconfig: 'udevdir'), 'rules.d'))
endif

# generate config.h
configure_file(output : 'config.h',
  configuration : conf_data)
