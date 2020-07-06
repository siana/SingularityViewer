#!/usr/bin/env python
"""\
@file viewer_manifest.py
@author Ryan Williams
@brief Description of all installer viewer files, and methods for packaging
       them into installers for all supported platforms.

$LicenseInfo:firstyear=2006&license=viewerlgpl$
Second Life Viewer Source Code
Copyright (C) 2006-2014, Linden Research, Inc.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
version 2.1 of the License only.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
$/LicenseInfo$
"""
import errno
import json
import os
import os.path
import plistlib
import random
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import time
import zipfile

viewer_dir = os.path.dirname(__file__)
# Add indra/lib/python to our path so we don't have to muck with PYTHONPATH.
# Put it FIRST because some of our build hosts have an ancient install of
# indra.util.llmanifest under their system Python!
sys.path.insert(0, os.path.join(viewer_dir, os.pardir, "lib", "python"))
from indra.util.llmanifest import LLManifest, main, proper_windows_path, path_ancestors, CHANNEL_VENDOR_BASE, RELEASE_CHANNEL, ManifestError
from llbase import llsd

class ViewerManifest(LLManifest):
    def is_packaging_viewer(self):
        # Some commands, files will only be included
        # if we are packaging the viewer on windows.
        # This manifest is also used to copy
        # files during the build (see copy_w_viewer_manifest
        # and copy_l_viewer_manifest targets)
        return 'package' in self.args['actions']

    def package_skin(self, xml, skin_dir):
        self.path(xml)
        self.path(skin_dir + "/*")
        # include the entire textures directory recursively
        with self.prefix(src_dst=skin_dir+"/textures"):
            self.path("*/*.tga")
            self.path("*/*.j2c")
            self.path("*/*.jpg")
            self.path("*/*.png")
            self.path("*.tga")
            self.path("*.j2c")
            self.path("*.jpg")
            self.path("*.png")
            self.path("*.xml")

    def construct(self):
        super(ViewerManifest, self).construct()
        self.path(src="../../scripts/messages/message_template.msg", dst="app_settings/message_template.msg")
        self.path(src="../../etc/message.xml", dst="app_settings/message.xml")

        if True: #self.is_packaging_viewer():
            with self.prefix(src_dst="app_settings"):
                self.exclude("logcontrol.xml")
                self.exclude("logcontrol-dev.xml")
                self.path("*.crt")
                self.path("*.ini")
                self.path("*.xml")
                self.path("*.db2")

                # include the entire shaders directory recursively
                self.path("shaders")

                # ... and the entire windlight directory
                self.path("windlight")

                # ... and the included spell checking dictionaries
                pkgdir = os.path.join(self.args['build'], os.pardir, 'packages')
                with self.prefix(src=pkgdir):
                    self.path("dictionaries")

                # include the extracted packages information (see BuildPackagesInfo.cmake)
                self.path(src=os.path.join(self.args['build'],"packages-info.txt"), dst="packages-info.txt")


            with self.prefix(src_dst="character"):
                self.path("*.llm")
                self.path("*.xml")
                self.path("*.tga")

            # Include our fonts
            with self.prefix(src=os.path.join(pkgdir, "fonts"), dst="fonts"):
                self.path("DejaVuSansCondensed*.ttf")
                self.path("DejaVuSansMono.ttf")
                self.path("DroidSans*.ttf")
                self.path("NotoSansDisplay*.ttf")
                self.path("SourceHanSans*.otf")

            # Include our font licenses
            with self.prefix(src_dst="fonts"):
                self.path("*.txt")

            # skins
            with self.prefix(src_dst="skins"):
                self.path("paths.xml")
                self.path("default/xui/*/*.xml")
                self.package_skin("Default.xml", "default")
                self.package_skin("dark.xml", "dark")
                self.package_skin("Gemini.xml", "gemini")

                # Local HTML files (e.g. loading screen)
                with self.prefix(src_dst="*/html"):
                    self.path("*.png")
                    self.path("*/*/*.html")
                    self.path("*/*/*.gif")

            # File in the newview/ directory
            self.path("gpu_table.txt")

            #build_data.json.  Standard with exception handling is fine.  If we can't open a new file for writing, we have worse problems
            #platform is computed above with other arg parsing
            build_data_dict = {"Type":"viewer","Version":'.'.join(self.args['version']),
                            "Channel Base": CHANNEL_VENDOR_BASE,
                            "Channel":self.channel_with_pkg_suffix(),
                            "Platform":self.build_data_json_platform,
                            "Address Size":self.address_size,
                            "Update Service":"https://app.alchemyviewer.org/update",
                            }
            build_data_dict = self.finish_build_data_dict(build_data_dict)
            with open(os.path.join(os.pardir,'build_data.json'), 'w') as build_data_handle:
                json.dump(build_data_dict,build_data_handle)

            #we likely no longer need the test, since we will throw an exception above, but belt and suspenders and we get the
            #return code for free.
            if not self.path2basename(os.pardir, "build_data.json"):
                print "No build_data.json file"

    def standalone(self):
        return self.args['standalone'] == "ON"

    def finish_build_data_dict(self, build_data_dict):
        return build_data_dict

    def grid(self):
        return self.args['grid']

    def viewer_branding_id(self):
        return self.args['branding_id']

    def channel(self):
        return self.args['channel']

    def channel_with_pkg_suffix(self):
        fullchannel=self.channel()
        channel_suffix = self.args.get('channel_suffix')
        if channel_suffix:
            fullchannel+=' '+channel_suffix
        return fullchannel

    def channel_variant(self):
        global CHANNEL_VENDOR_BASE
        return self.channel().replace(CHANNEL_VENDOR_BASE, "").strip()

    def channel_type(self): # returns 'release', 'beta', 'project', or 'test'
        channel_qualifier=self.channel_variant().lower()
        if channel_qualifier.startswith('release'):
            channel_type='release'
        elif channel_qualifier.startswith('beta'):
            channel_type='beta'
        elif channel_qualifier.startswith('alpha'):
            channel_type='alpha'
        elif channel_qualifier.startswith('project'):
            channel_type='project'
        else:
            channel_type='test'
        return channel_type

    def channel_variant_app_suffix(self):
        # get any part of the channel name after the CHANNEL_VENDOR_BASE
        suffix=self.channel_variant()
        # by ancient convention, we don't use Release in the app name
        if self.channel_type() == 'release':
            suffix=suffix.replace('Release', '').strip()
        # for the base release viewer, suffix will now be null - for any other, append what remains
        if suffix:
            suffix = "_".join([''] + suffix.split())
        # the additional_packages mechanism adds more to the installer name (but not to the app name itself)
        # ''.split() produces empty list, so suffix only changes if
        # channel_suffix is non-empty
        suffix = "_".join([suffix] + self.args.get('channel_suffix', '').split())
        return suffix

    def installer_base_name(self):
        global CHANNEL_VENDOR_BASE
        # a standard map of strings for replacing in the templates
        substitution_strings = {
            'channel_vendor_base' : '_'.join(CHANNEL_VENDOR_BASE.split()),
            'channel_variant_underscores':self.channel_variant_app_suffix(),
            'version_underscores' : '_'.join(self.args['version']),
            'arch':self.args['arch']
            }
        return "%(channel_vendor_base)s%(channel_variant_underscores)s_%(version_underscores)s_%(arch)s" % substitution_strings

    def app_name(self):
        global CHANNEL_VENDOR_BASE
        channel_type=self.channel_type()
        if channel_type == 'release':
            app_suffix=''
        else:
            app_suffix=self.channel_variant()
        return CHANNEL_VENDOR_BASE + ' ' + app_suffix

    def app_name_oneword(self):
        return ''.join(self.app_name().split())

    def icon_path(self):
        return "icons/" + ("default", "alpha")[self.channel_type() == "alpha"]

    def extract_names(self,src):
        try:
            contrib_file = open(src,'r')
        except IOError:
            print "Failed to open '%s'" % src
            raise
        lines = contrib_file.readlines()
        contrib_file.close()

        # All lines up to and including the first blank line are the file header; skip them
        lines.reverse() # so that pop will pull from first to last line
        while not re.match("\s*$", lines.pop()) :
            pass # do nothing

        # A line that starts with a non-whitespace character is a name; all others describe contributions, so collect the names
        names = []
        for line in lines :
            if re.match("\S", line) :
                names.append(line.rstrip())
        # It's not fair to always put the same people at the head of the list
        random.shuffle(names)
        return ', '.join(names)

    def relsymlinkf(self, src, dst=None, catch=True):
        """
        relsymlinkf() is just like symlinkf(), but instead of requiring the
        caller to pass 'src' as a relative pathname, this method expects 'src'
        to be absolute, and creates a symlink whose target is the relative
        path from 'src' to dirname(dst).
        """
        dstdir, dst = self._symlinkf_prep_dst(src, dst)

        # Determine the relative path starting from the directory containing
        # dst to the intended src.
        src = self.relpath(src, dstdir)

        self._symlinkf(src, dst, catch)
        return dst

    def symlinkf(self, src, dst=None, catch=True):
        """
        Like ln -sf, but uses os.symlink() instead of running ln. This creates
        a symlink at 'dst' that points to 'src' -- see:
        https://docs.python.org/2/library/os.html#os.symlink

        If you omit 'dst', this creates a symlink with basename(src) at
        get_dst_prefix() -- in other words: put a symlink to this pathname
        here at the current dst prefix.

        'src' must specifically be a *relative* symlink. It makes no sense to
        create an absolute symlink pointing to some path on the build machine!

        Also:
        - We prepend 'dst' with the current get_dst_prefix(), so it has similar
          meaning to associated self.path() calls.
        - We ensure that the containing directory os.path.dirname(dst) exists
          before attempting the symlink.

        If you pass catch=False, exceptions will be propagated instead of
        caught.
        """
        dstdir, dst = self._symlinkf_prep_dst(src, dst)
        self._symlinkf(src, dst, catch)
        return dst

    def _symlinkf_prep_dst(self, src, dst):
        # helper for relsymlinkf() and symlinkf()
        if dst is None:
            dst = os.path.basename(src)
        dst = os.path.join(self.get_dst_prefix(), dst)
        # Seems silly to prepend get_dst_prefix() to dst only to call
        # os.path.dirname() on it again, but this works even when the passed
        # 'dst' is itself a pathname.
        dstdir = os.path.dirname(dst)
        self.cmakedirs(dstdir)
        return (dstdir, dst)

    def _symlinkf(self, src, dst, catch):
        # helper for relsymlinkf() and symlinkf()
        # the passed src must be relative
        if os.path.isabs(src):
            raise ManifestError("Do not symlinkf(absolute %r, asis=True)" % src)

        # The outer catch is the one that reports failure even after attempted
        # recovery.
        try:
            # At the inner layer, recovery may be possible.
            try:
                os.symlink(src, dst)
            except OSError as err:
                if err.errno != errno.EEXIST:
                    raise
                # We could just blithely attempt to remove and recreate the target
                # file, but that strategy doesn't work so well if we don't have
                # permissions to remove it. Check to see if it's already the
                # symlink we want, which is the usual reason for EEXIST.
                elif os.path.islink(dst):
                    if os.readlink(dst) == src:
                        # the requested link already exists
                        pass
                    else:
                        # dst is the wrong symlink; attempt to remove and recreate it
                        os.remove(dst)
                        os.symlink(src, dst)
                elif os.path.isdir(dst):
                    print "Requested symlink (%s) exists but is a directory; replacing" % dst
                    shutil.rmtree(dst)
                    os.symlink(src, dst)
                elif os.path.exists(dst):
                    print "Requested symlink (%s) exists but is a file; replacing" % dst
                    os.remove(dst)
                    os.symlink(src, dst)
                else:
                    # out of ideas
                    raise
        except Exception as err:
            # report
            print "Can't symlink %r -> %r: %s: %s" % \
                  (dst, src, err.__class__.__name__, err)
            # if caller asked us not to catch, re-raise this exception
            if not catch:
                raise

    def relpath(self, path, base=None, symlink=False):
        """
        Return the relative path from 'base' to the passed 'path'. If base is
        omitted, self.get_dst_prefix() is assumed. In other words: make a
        same-name symlink to this path right here in the current dest prefix.

        Normally we resolve symlinks. To retain symlinks, pass symlink=True.
        """
        if base is None:
            base = self.get_dst_prefix()

        # Since we use os.path.relpath() for this, which is purely textual, we
        # must ensure that both pathnames are absolute.
        if symlink:
            # symlink=True means: we know path is (or indirects through) a
            # symlink, don't resolve, we want to use the symlink.
            abspath = os.path.abspath
        else:
            # symlink=False means to resolve any symlinks we may find
            abspath = os.path.realpath

        return os.path.relpath(abspath(path), abspath(base))


class WindowsManifest(ViewerManifest):
    # We want the platform, per se, for every Windows build to be 'win'. The
    # VMP will concatenate that with the address_size.
    build_data_json_platform = 'win'

    def final_exe(self):
        return self.app_name_oneword()+"Viewer.exe"

    def finish_build_data_dict(self, build_data_dict):
        #MAINT-7294: Windows exe names depend on channel name, so write that in also
        build_data_dict['Executable'] = self.final_exe()
        build_data_dict['AppName']    = self.app_name()
        return build_data_dict

    def test_msvcrt_and_copy_action(self, src, dst):
        # This is used to test a dll manifest.
        # It is used as a temporary override during the construct method
        from test_win32_manifest import test_assembly_binding
        # TODO: This is redundant with LLManifest.copy_action(). Why aren't we
        # calling copy_action() in conjunction with test_assembly_binding()?
        if src and (os.path.exists(src) or os.path.islink(src)):
            # ensure that destination path exists
            self.cmakedirs(os.path.dirname(dst))
            self.created_paths.append(dst)
            if not os.path.isdir(src):
                if(self.args['configuration'].lower() == 'debug'):
                    test_assembly_binding(src, "Microsoft.VC80.DebugCRT", "8.0.50727.4053")
                else:
                    test_assembly_binding(src, "Microsoft.VC80.CRT", "8.0.50727.4053")
                self.ccopy(src,dst)
            else:
                raise Exception("Directories are not supported by test_CRT_and_copy_action()")
        else:
            print "Doesn't exist:", src

    def test_for_no_msvcrt_manifest_and_copy_action(self, src, dst):
        # This is used to test that no manifest for the msvcrt exists.
        # It is used as a temporary override during the construct method
        from test_win32_manifest import test_assembly_binding
        from test_win32_manifest import NoManifestException, NoMatchingAssemblyException
        # TODO: This is redundant with LLManifest.copy_action(). Why aren't we
        # calling copy_action() in conjunction with test_assembly_binding()?
        if src and (os.path.exists(src) or os.path.islink(src)):
            # ensure that destination path exists
            self.cmakedirs(os.path.dirname(dst))
            self.created_paths.append(dst)
            if not os.path.isdir(src):
                try:
                    if(self.args['configuration'].lower() == 'debug'):
                        test_assembly_binding(src, "Microsoft.VC80.DebugCRT", "")
                    else:
                        test_assembly_binding(src, "Microsoft.VC80.CRT", "")
                    raise Exception("Unknown condition")
                except NoManifestException as err:
                    pass
                except NoMatchingAssemblyException as err:
                    pass

                self.ccopy(src,dst)
            else:
                raise Exception("Directories are not supported by test_CRT_and_copy_action()")
        else:
            print "Doesn't exist:", src

    def construct(self):
        super(WindowsManifest, self).construct()

        if self.args['configuration'].lower() == '.':
            config = 'debug' if self.args['buildtype'].lower() == 'debug' else 'release'
        else:
            config = 'debug' if self.args['configuration'].lower() == 'debug' else 'release'
        pkgdir = os.path.join(self.args['build'], os.pardir, 'packages')
        relpkgdir = os.path.join(pkgdir, "lib", "release")
        debpkgdir = os.path.join(pkgdir, "lib", "debug")
        pkgbindir = os.path.join(pkgdir, "bin", config)

        if True: #self.is_packaging_viewer():
            # Find singularity-bin.exe in the 'configuration' dir, then rename it to the result of final_exe.
            self.path(src=os.path.join(self.args['dest'], ('%s-bin.exe' % self.viewer_branding_id())), dst=self.final_exe())

        # Plugin host application
        self.path2basename(os.path.join(os.pardir,
                                        'llplugin', 'slplugin', config),
                           "SLplugin.exe")

        # Get shared libs from the shared libs staging directory
        with self.prefix(src=os.path.join(self.args['build'], os.pardir,
                                          'sharedlibs', config)):

            # Get llcommon and deps. If missing assume static linkage and continue.
            if self.path('llcommon.dll') == 0:
                print "Skipping llcommon.dll (assuming llcommon was linked statically)"

            self.path('libapr-1.dll')
            self.path('libaprutil-1.dll')
            self.path('libapriconv-1.dll')

            # Mesh 3rd party libs needed for auto LOD and collada reading
            if self.path("glod.dll") == 0:
                print "Skipping GLOD library (assumming linked statically)"

            # Get fmodstudio dll, continue if missing
            if config is "debug":
                if self.path("fmodL.dll") == 0:
                   print "Skipping fmodstudio audio library(assuming other audio engine)"
            else:
                if self.path("fmod.dll") == 0:
                   print "Skipping fmodstudio audio library(assuming other audio engine)"

            # Get OpenAL dlls, continue if missing
            if self.path("alut.dll","OpenAL32.dll") == 0:
                print "Skipping OpenAL audio library (assuming other audio engine)"

            # Vivox runtimes
            self.path("SLVoice.exe")
            if (self.address_size == 64):
                self.path("vivoxsdk_x64.dll")
                self.path("ortp_x64.dll")
            else:
                self.path("vivoxsdk.dll")
                self.path("ortp.dll")
            #self.path("libsndfile-1.dll")
            #self.path("vivoxoal.dll")
            
            # Security
            if(self.address_size == 64):
                self.path("libcrypto-1_1-x64.dll")
                self.path("libssl-1_1-x64.dll")
                #if not self.is_packaging_viewer():
                #    self.path("libcrypto-1_1-x64.pdb")
                #    self.path("libssl-1_1-x64.pdb")
            else:
                self.path("libcrypto-1_1.dll")
                self.path("libssl-1_1.dll")
                #if not self.is_packaging_viewer():
                #    self.path("libcrypto-1_1.pdb")
                #    self.path("libssl-1_1.pdb")

            # Hunspell
            self.path("libhunspell.dll")

            # For google-perftools tcmalloc allocator.
            if(self.address_size == 32):
                if config == 'debug':
                    if self.path('libtcmalloc_minimal-debug.dll') == 0:
                        print "Skipping libtcmalloc_minimal.dll"
                else:
                    if self.path('libtcmalloc_minimal.dll') == 0:
                        print "Skipping libtcmalloc_minimal.dll"

        # For crashpad
        with self.prefix(src=pkgbindir):
            self.path("crashpad_handler.exe")
            if not self.is_packaging_viewer():
                self.path("crashpad_handler.pdb")

        self.path(src="licenses-win32.txt", dst="licenses.txt")
        self.path("featuretable.txt")

        # Plugins
        with self.prefix(dst="llplugin"):
            with self.prefix(src=os.path.join(self.args['build'], os.pardir, 'plugins')):

                # Plugins - FilePicker
                with self.prefix(src=os.path.join('filepicker', config)):
                    self.path("basic_plugin_filepicker.dll")

                # Media plugins - LibVLC
                with self.prefix(src=os.path.join('libvlc', config)):
                    self.path("media_plugin_libvlc.dll")

                # Media plugins - CEF
                with self.prefix(src=os.path.join('cef', config)):
                    self.path("media_plugin_cef.dll")

            # CEF runtime files - debug
            # CEF runtime files - not debug (release, relwithdebinfo etc.)
            with self.prefix(src=pkgbindir):
                self.path("chrome_elf.dll")
                self.path("d3dcompiler_43.dll")
                self.path("d3dcompiler_47.dll")
                self.path("libcef.dll")
                self.path("libEGL.dll")
                self.path("libGLESv2.dll")
                self.path("dullahan_host.exe")
                self.path("natives_blob.bin")
                self.path("snapshot_blob.bin")
                self.path("v8_context_snapshot.bin")

            with self.prefix(src=os.path.join(pkgbindir, 'swiftshader'), dst='swiftshader'):
                self.path("libEGL.dll")
                self.path("libGLESv2.dll")

            # CEF files common to all configurations
            with self.prefix(src=os.path.join(pkgdir, 'resources')):
                self.path("cef.pak")
                self.path("cef_100_percent.pak")
                self.path("cef_200_percent.pak")
                self.path("cef_extensions.pak")
                self.path("devtools_resources.pak")
                self.path("icudtl.dat")

            with self.prefix(src=os.path.join(pkgdir, 'resources', 'locales'), dst='locales'):
                self.path("am.pak")
                self.path("ar.pak")
                self.path("bg.pak")
                self.path("bn.pak")
                self.path("ca.pak")
                self.path("cs.pak")
                self.path("da.pak")
                self.path("de.pak")
                self.path("el.pak")
                self.path("en-GB.pak")
                self.path("en-US.pak")
                self.path("es-419.pak")
                self.path("es.pak")
                self.path("et.pak")
                self.path("fa.pak")
                self.path("fi.pak")
                self.path("fil.pak")
                self.path("fr.pak")
                self.path("gu.pak")
                self.path("he.pak")
                self.path("hi.pak")
                self.path("hr.pak")
                self.path("hu.pak")
                self.path("id.pak")
                self.path("it.pak")
                self.path("ja.pak")
                self.path("kn.pak")
                self.path("ko.pak")
                self.path("lt.pak")
                self.path("lv.pak")
                self.path("ml.pak")
                self.path("mr.pak")
                self.path("ms.pak")
                self.path("nb.pak")
                self.path("nl.pak")
                self.path("pl.pak")
                self.path("pt-BR.pak")
                self.path("pt-PT.pak")
                self.path("ro.pak")
                self.path("ru.pak")
                self.path("sk.pak")
                self.path("sl.pak")
                self.path("sr.pak")
                self.path("sv.pak")
                self.path("sw.pak")
                self.path("ta.pak")
                self.path("te.pak")
                self.path("th.pak")
                self.path("tr.pak")
                self.path("uk.pak")
                self.path("vi.pak")
                self.path("zh-CN.pak")
                self.path("zh-TW.pak")

            with self.prefix(src=pkgbindir):
                self.path("libvlc.dll")
                self.path("libvlccore.dll")
                self.path("plugins/")


        if not self.is_packaging_viewer():
            self.package_file = "copied_deps"

    def nsi_file_commands(self, install=True):
        def wpath(path):
            if path.endswith('/') or path.endswith(os.path.sep):
                path = path[:-1]
            path = path.replace('/', '\\')
            return path

        result = ""
        dest_files = [pair[1] for pair in self.file_list if pair[0] and os.path.isfile(pair[1])]
        # sort deepest hierarchy first
        dest_files.sort(lambda a,b: cmp(a.count(os.path.sep),b.count(os.path.sep)) or cmp(a,b))
        dest_files.reverse()
        out_path = None
        for pkg_file in dest_files:
            rel_file = os.path.normpath(pkg_file.replace(self.get_dst_prefix()+os.path.sep,''))
            if install and rel_file.startswith(("llplugin\\libvlc", "llplugin\\plugins\\")):
                continue
            installed_dir = wpath(os.path.join('$INSTDIR', os.path.dirname(rel_file)))
            pkg_file = wpath(os.path.normpath(pkg_file))
            if installed_dir != out_path:
                if install:
                    out_path = installed_dir
                    result += 'SetOutPath ' + out_path + '\n'
            if install:
                result += 'File "' + pkg_file + '"\n'
            else:
                result += 'Delete "' + wpath(os.path.join('$INSTDIR', rel_file)) + '"\n'

        # at the end of a delete, just rmdir all the directories
        if not install:
            deleted_file_dirs = [os.path.dirname(pair[1].replace(self.get_dst_prefix()+os.path.sep,'')) for pair in self.file_list]
            # find all ancestors so that we don't skip any dirs that happened to have no non-dir children
            deleted_dirs = []
            for d in deleted_file_dirs:
                deleted_dirs.extend(path_ancestors(d))
            # sort deepest hierarchy first
            deleted_dirs.sort(lambda a,b: cmp(a.count(os.path.sep),b.count(os.path.sep)) or cmp(a,b))
            deleted_dirs.reverse()
            prev = None
            for d in deleted_dirs:
                if d != prev:   # skip duplicates
                    result += 'RMDir ' + wpath(os.path.join('$INSTDIR', os.path.normpath(d))) + '\n'
                prev = d

        return result
	
    def sign_command(self, *argv):
        return [
            "signtool.exe", "sign", "/v",
            "/n", self.args['signature'],
            "/p", os.environ['VIEWER_SIGNING_PWD'],
            "/d","%s" % self.channel(),
            "/t","http://timestamp.comodoca.com/authenticode"
        ] + list(argv)
	
    def sign(self, *argv):
        subprocess.check_call(self.sign_command(*argv))

    def package_finish(self):
        if 'signature' in self.args and 'VIEWER_SIGNING_PWD' in os.environ:
            try:
                self.sign(self.args['dest']+"\\"+self.final_exe())
                self.sign(self.args['dest']+"\\SLPlugin.exe")
                self.sign(self.args['dest']+"\\SLVoice.exe")
            except:
                print "Couldn't sign binaries. Tried to sign %s" % self.args['dest'] + "\\" + self.final_exe()
		
        # a standard map of strings for replacing in the templates
        substitution_strings = {
            'version' : '.'.join(self.args['version']),
            'version_short' : '.'.join(self.args['version'][:-1]),
            'version_dashes' : '-'.join(self.args['version']),
            'version_registry' : '%s(%s)' %
            ('.'.join(self.args['version']), self.address_size),
            'final_exe' : self.final_exe(),
            'flags':'',
            'app_name':self.app_name(),
            'app_name_oneword':self.app_name_oneword()
            }

        installer_file = self.installer_base_name() + '_Setup.exe'
        substitution_strings['installer_file'] = installer_file

        # Packaging the installer takes forever, dodge it if we can.
        installer_path = os.path.join(self.args['dest'], installer_file);
        if os.path.isfile(installer_path):
            binary_mod = os.path.getmtime(os.path.join(self.args['dest'], self.final_exe()))
            installer_mod = os.path.getmtime(installer_path)
            if binary_mod <= installer_mod:
                print("Binary is unchanged since last package, touch the binary or delete installer to trigger repackage.")
                exit();

        version_vars = """
        !define INSTEXE  "%(final_exe)s"
        !define VERSION "%(version_short)s"
        !define VERSION_LONG "%(version)s"
        !define VERSION_DASHES "%(version_dashes)s"
        """ % substitution_strings

        if self.channel_type() == 'release':
            substitution_strings['caption'] = CHANNEL_VENDOR_BASE
        else:
            substitution_strings['caption'] = self.app_name() + ' ${VERSION}'

        inst_vars_template = """
            !define INSTEXE  "%(final_exe)s"
            !define INSTOUTFILE "%(installer_file)s"
            !define APPNAME   "%(app_name)s"
            !define APPNAMEONEWORD   "%(app_name_oneword)s"
            !define URLNAME   "secondlife"
            !define CAPTIONSTR "%(caption)s"
            !define VENDORSTR "Singularity Viewer Project"
            !define VERSION "%(version_short)s"
            !define VERSION_LONG "%(version)s"
            !define VERSION_DASHES "%(version_dashes)s"
            """

        tempfile = "%s_setup_tmp.nsi" % self.viewer_branding_id()
        # the following replaces strings in the nsi template
        # it also does python-style % substitution
        self.replace_in("installers/windows/installer_template.nsi", tempfile, {
                "%%VERSION%%":version_vars,
                "%%SOURCE%%":self.get_src_prefix(),
                "%%INST_VARS%%":inst_vars_template % substitution_strings,
                "%%INSTALL_FILES%%":self.nsi_file_commands(True),
                "%%DELETE_FILES%%":self.nsi_file_commands(False),
                "%%WIN64_BIN_BUILD%%":"!define WIN64_BIN_BUILD 1" if (self.address_size == 64) else "",})

        # We use the Unicode version of NSIS, available from
        # http://www.scratchpaper.com/
        try:
            import _winreg as reg
            NSIS_path = reg.QueryValue(reg.HKEY_LOCAL_MACHINE, r"SOFTWARE\NSIS") + '\\makensis.exe'
            self.run_command([proper_windows_path(NSIS_path), self.dst_path_of(tempfile)])
        except:
            try:
                NSIS_path = os.environ['ProgramFiles'] + '\\NSIS\\makensis.exe'
                self.run_command([proper_windows_path(NSIS_path), self.dst_path_of(tempfile)])
            except:
                NSIS_path = os.environ['ProgramFiles(X86)'] + '\\NSIS\\makensis.exe'
                self.run_command([proper_windows_path(NSIS_path),self.dst_path_of(tempfile)])
        self.remove(self.dst_path_of(tempfile))
        if 'signature' in self.args and 'VIEWER_SIGNING_PWD' in os.environ:
            try:
                self.sign(self.args['dest'] + "\\" + substitution_strings['installer_file'])
            except: 
                print "Couldn't sign windows installer. Tried to sign %s" % self.args['dest'] + "\\" + substitution_strings['installer_file']

        self.created_path(self.dst_path_of(installer_file))
        self.package_file = installer_file


class Windows_i686_Manifest(WindowsManifest):
    # Although we aren't literally passed ADDRESS_SIZE, we can infer it from
    # the passed 'arch', which is used to select the specific subclass.
    address_size = 32

class Windows_x86_64_Manifest(WindowsManifest):
    address_size = 64


class DarwinManifest(ViewerManifest):
    build_data_json_platform = 'mac'

    def finish_build_data_dict(self, build_data_dict):
        build_data_dict.update({'Bundle Id':self.args['bundleid']})
        return build_data_dict

    def is_packaging_viewer(self):
        # darwin requires full app bundle packaging even for debugging.
        return True

    def is_rearranging(self):
        # That said, some stuff should still only be performed once.
        # Are either of these actions in 'actions'? Is the set intersection
        # non-empty?
        return bool(set(["package", "unpacked"]).intersection(self.args['actions']))

    def construct(self):
        # copy over the build result (this is a no-op if run within the xcode script)
        self.path(os.path.join(self.args['configuration'], self.app_name()+".app"), dst="")

        with self.prefix(src="", dst="Contents"):  # everything goes in Contents

            # most everything goes in the Resources directory
            with self.prefix(src="", dst="Resources"):
                super(DarwinManifest, self).construct()

                with self.prefix(src_dst="cursors_mac"):
                    self.path("*.tif")

                self.path("licenses-mac.txt", dst="licenses.txt")
                self.path("featuretable_mac.txt")
                self.path("SecondLife.nib")

                icon_path = self.icon_path()
                with self.prefix(src=icon_path, dst="") :
                    self.path("%s.icns" % self.viewer_branding_id())

                # Translations
                self.path("English.lproj")
                self.path("German.lproj")
                self.path("Japanese.lproj")
                self.path("Korean.lproj")
                self.path("da.lproj")
                self.path("es.lproj")
                self.path("fr.lproj")
                self.path("hu.lproj")
                self.path("it.lproj")
                self.path("nl.lproj")
                self.path("pl.lproj")
                self.path("pt.lproj")
                self.path("ru.lproj")
                self.path("tr.lproj")
                self.path("uk.lproj")
                self.path("zh-Hans.lproj")

                libdir = "../packages/lib/release"
                alt_libdir = "../packages/libraries/universal-darwin/lib/release"
                # dylibs is a list of all the .dylib files we expect to need
                # in our bundled sub-apps. For each of these we'll create a
                # symlink from sub-app/Contents/Resources to the real .dylib.
                # Need to get the llcommon dll from any of the build directories as well.
                libfile = "libllcommon.dylib"

                dylibs = self.path_optional(self.find_existing_file(os.path.join(os.pardir,
                                                               "llcommon",
                                                               self.args['configuration'],
                                                               libfile),
                                                               os.path.join(libdir, libfile)),
                                       dst=libfile)

                with self.prefix(src=libdir, alt_build=alt_libdir, dst=""):
                    for libfile in (
                                    "libapr-1.0.dylib",
                                    "libaprutil-1.0.dylib",
                                    "libcollada14dom.dylib",
                                    "libexpat.1.5.2.dylib",
                                    "libexception_handler.dylib",
                                    "libGLOD.dylib",
                                    "libhunspell-1.3.0.dylib",
                                    "libndofdev.dylib",
                                    ):
                        dylibs += self.path_optional(libfile)

                # SLVoice and vivox lols, no symlinks needed
                    for libfile in (
                                'libortp.dylib',
                                'libsndfile.dylib',
                                'libvivoxoal.dylib',
                                'libvivoxsdk.dylib',
                                'libvivoxplatform.dylib',
                                'ca-bundle.crt',
                                'SLVoice'
                                ):
                        self.path(libfile)

                with self.prefix(src= '', alt_build=libdir):
                    dylibs += self.add_extra_libraries()

                # our apps
                for app_bld_dir, app in (#("mac_crash_logger", "mac-crash-logger.app"),
                                         # plugin launcher
                                         (os.path.join("llplugin", "slplugin"), "SLPlugin.app"),
                                         ):
                    self.path2basename(os.path.join(os.pardir,
                                                    app_bld_dir, self.args['configuration']),
                                       app)

                    # our apps dependencies on shared libs
                    # for each app, for each dylib we collected in dylibs,
                    # create a symlink to the real copy of the dylib.
                    resource_path = self.dst_path_of(os.path.join(app, "Contents", "Resources"))
                    for libfile in dylibs:
                        src = os.path.join(os.pardir, os.pardir, os.pardir, libfile)
                        dst = os.path.join(resource_path, libfile)
                        try:
                            symlinkf(src, dst)
                        except OSError as err:
                            print "Can't symlink %s -> %s: %s" % (src, dst, err)

                # Dullahan helper apps go inside AlchemyPlugin.app
                with self.prefix(src="", dst="AlchemyPlugin.app/Contents/Frameworks"):
                    helperappfile = 'DullahanHelper.app'
                    self.path2basename(relbinpkgdir, helperappfile)

                    pluginframeworkpath = self.dst_path_of('Chromium Embedded Framework.framework');
                    # Putting a Frameworks directory under Contents/MacOS
                    # isn't canonical, but the path baked into Dullahan
                    # Helper.app/Contents/MacOS/DullahanHelper is:
                    # @executable_path/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework
                    # (notice, not @executable_path/../Frameworks/etc.)
                    # So we'll create a symlink (below) from there back to the
                    # Frameworks directory nested under SLPlugin.app.
                    helperframeworkpath = \
                        self.dst_path_of('DullahanHelper.app/Contents/MacOS/'
                                         'Frameworks/Chromium Embedded Framework.framework')

                    helperexecutablepath = self.dst_path_of('DullahanHelper.app/Contents/MacOS/DullahanHelper')
                    self.run_command(['install_name_tool', '-change',
                                     '@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework',
                                     '@executable_path/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework', helperexecutablepath])

                # plugins
                with self.prefix(src="", dst="llplugin"):
                    self.path2basename("../media_plugins/quicktime/" + self.args['configuration'],
                                       "media_plugin_quicktime.dylib")
                    self.path2basename("../media_plugins/cef/" + self.args['configuration'],
                                       "media_plugin_cef.dylib")

                # command line arguments for connecting to the proper grid
                self.put_in_file(self.flags_list(), 'arguments.txt')

                # CEF framework goes inside Second Life.app/Contents/Frameworks
                with self.prefix(src="", dst="Frameworks"):
                    frameworkfile="Chromium Embedded Framework.framework"
                    self.path2basename(relpkgdir, frameworkfile)

                # This code constructs a relative path from the
                # target framework folder back to the location of the symlink.
                # It needs to be relative so that the symlink still works when
                # (as is normal) the user moves the app bundle out of the DMG
                # and into the /Applications folder. Note we also call 'raise'
                # to terminate the process if we get an error since without
                # this symlink, Second Life web media can't possibly work.
                # Real Framework folder:
                #   Alchemy.app/Contents/Frameworks/Chromium Embedded Framework.framework/
                # Location of symlink and why it's relative 
                #   Alchemy.app/Contents/Resources/AlchemyPlugin.app/Contents/Frameworks/Chromium Embedded Framework.framework/
                # Real Frameworks folder, with the symlink inside the bundled SLPlugin.app (and why it's relative)
                #   <top level>.app/Contents/Frameworks/Chromium Embedded Framework.framework/
                #   <top level>.app/Contents/Resources/AlchemyPlugin.app/Contents/Frameworks/Chromium Embedded Framework.framework ->
                # It might seem simpler just to create a symlink Frameworks to
                # the parent of Chromimum Embedded Framework.framework. But
                # that would create a symlink cycle, which breaks our
                # packaging step. So make a symlink from Chromium Embedded
                # Framework.framework to the directory of the same name, which
                # is NOT an ancestor of the symlink.
                frameworkpath = os.path.join(os.pardir, os.pardir, os.pardir, 
                                             os.pardir, "Frameworks",
                                             "Chromium Embedded Framework.framework")
                try:
                    # from AlchemyPlugin.app/Contents/Frameworks/Chromium Embedded
                    # Framework.framework back to
                    # Alchemy.app/Contents/Frameworks/Chromium Embedded Framework.framework
                    origin, target = pluginframeworkpath, frameworkpath
                    self.symlinkf(target, origin)
                    # from AlchemyPlugin.app/Contents/Frameworks/Dullahan
                    # Helper.app/Contents/MacOS/Frameworks/Chromium Embedded
                    # Framework.framework back to
                    # AlchemyPlugin.app/Contents/Frameworks/Chromium Embedded Framework.framework
                    self.cmakedirs(os.path.dirname(helperframeworkpath))
                    origin = helperframeworkpath
                    target = os.path.join(os.pardir, frameworkpath)
                    self.symlinkf(target, origin)
                except OSError as err:
                    print "Can't symlink %s -> %s: %s" % (origin, target, err)
                    raise

        # NOTE: the -S argument to strip causes it to keep enough info for
        # annotated backtraces (i.e. function names in the crash log).  'strip' with no
        # arguments yields a slightly smaller binary but makes crash logs mostly useless.
        # This may be desirable for the final release.  Or not.
        if self.buildtype().lower()=='release':
            if ("package" in self.args['actions'] or
                "unpacked" in self.args['actions']):
                self.run_command('strip -S "%(viewer_binary)s"' %
                                 { 'viewer_binary' : self.dst_path_of('Contents/MacOS/'+self.app_name())})

    def app_name(self):
        return self.channel_oneword()

    def package_finish(self):
        channel_standin = self.app_name()
        if not self.default_channel_for_brand():
            channel_standin = self.channel()

        # Sign the app if we have a key.
        try:
            signing_password = os.environ['VIEWER_SIGNING_PASSWORD']
        except KeyError:
            print "Skipping code signing"
            pass
        else:
            home_path = os.environ['HOME']

            self.run_command('security unlock-keychain -p "%s" "%s/Library/Keychains/viewer.keychain"' % (signing_password, home_path))
            signed=False
            sign_attempts=3
            sign_retry_wait=15
            while (not signed) and (sign_attempts > 0):
                try:
                    sign_attempts-=1;
                    self.run_command('codesign --verbose --force --timestamp --keychain "%(home_path)s/Library/Keychains/viewer.keychain" -s %(identity)r -f %(bundle)r' % {
                            'home_path' : home_path,
                            'identity': os.environ['VIEWER_SIGNING_KEY'],
                            'bundle': self.get_dst_prefix()
                    })
                    signed=True
                except:
                    if sign_attempts:
                        print >> sys.stderr, "codesign failed, waiting %d seconds before retrying" % sign_retry_wait
                        time.sleep(sign_retry_wait)
                        sign_retry_wait*=2
                    else:
                        print >> sys.stderr, "Maximum codesign attempts exceeded; giving up"
                        raise

        imagename=self.installer_prefix() + '_'.join(self.args['version'])

        # See Ambroff's Hack comment further down if you want to create new bundles and dmg
        volname=self.app_name() + " Installer"  # DO NOT CHANGE without checking Ambroff's Hack comment further down

        if self.default_channel_for_brand():
            if not self.default_grid():
                # beta case
                imagename = imagename + '_' + self.args['grid'].upper()
        else:
            # first look, etc
            imagename = imagename + '_' + self.channel_oneword().upper()

        sparsename = imagename + ".sparseimage"
        finalname = imagename + ".dmg"
        # make sure we don't have stale files laying about
        self.remove(sparsename, finalname)

        self.run_command('hdiutil create "%(sparse)s" -volname "%(vol)s" -fs HFS+ -type SPARSE -megabytes 700 -layout SPUD' % {
                'sparse':sparsename,
                'vol':volname})

        # mount the image and get the name of the mount point and device node
        hdi_output = self.run_command('hdiutil attach -private "' + sparsename + '"')
        devfile = re.search("/dev/disk([0-9]+)[^s]", hdi_output).group(0).strip()
        volpath = re.search('HFS\s+(.+)', hdi_output).group(1).strip()

        # Copy everything in to the mounted .dmg

        if self.default_channel_for_brand() and not self.default_grid():
            app_name = self.app_name() + " " + self.args['grid']
        else:
            app_name = channel_standin.strip()

        # Hack:
        # Because there is no easy way to coerce the Finder into positioning
        # the app bundle in the same place with different app names, we are
        # adding multiple .DS_Store files to svn. There is one for release,
        # one for release candidate and one for first look. Any other channels
        # will use the release .DS_Store, and will look broken.
        # - Ambroff 2008-08-20
                # Added a .DS_Store for snowglobe - Merov 2009-06-17

                # We have a single branded installer for all snowglobe channels so snowglobe logic is a bit different
        if (self.app_name()=="Snowglobe"):
            dmg_template = os.path.join ('installers', 'darwin', 'snowglobe-dmg')
        else:
            dmg_template = os.path.join(
                'installers',
                'darwin',
                '%s-dmg' % "".join(self.channel_unique().split()).lower())

        if not os.path.exists (self.src_path_of(dmg_template)):
            dmg_template = os.path.join ('installers', 'darwin', 'release-dmg')

        
        for s,d in {self.build_path_of(self.get_dst_prefix()):app_name + ".app",
                    self.src_path_of(os.path.join(dmg_template, "_VolumeIcon.icns")): ".VolumeIcon.icns",
                    self.src_path_of(os.path.join(dmg_template, "background.jpg")): "background.jpg",
                    self.src_path_of(os.path.join(dmg_template, "_DS_Store")): ".DS_Store"}.items():
            print "Copying to dmg", s, d
            self.copy_action(s, os.path.join(volpath, d))

        # Hide the background image, DS_Store file, and volume icon file (set their "visible" bit)
        self.run_command('SetFile -a V "' + os.path.join(volpath, ".VolumeIcon.icns") + '"')
        self.run_command('SetFile -a V "' + os.path.join(volpath, "background.jpg") + '"')
        self.run_command('SetFile -a V "' + os.path.join(volpath, ".DS_Store") + '"')

        # Create the alias file (which is a resource file) from the .r
        self.run_command('rez "' + self.src_path_of("installers/darwin/release-dmg/Applications-alias.r") + '" -o "' + os.path.join(volpath, "Applications") + '"')

        # Set the alias file's alias and custom icon bits
        self.run_command('SetFile -a AC "' + os.path.join(volpath, "Applications") + '"')

        # Set the disk image root's custom icon bit
        self.run_command('SetFile -a C "' + volpath + '"')

        # Unmount the image
        self.run_command('hdiutil detach -force "' + devfile + '"')

        print "Converting temp disk image to final disk image"
        self.run_command('hdiutil convert "%(sparse)s" -format UDZO -imagekey zlib-level=9 -o "%(final)s"' % {'sparse':sparsename, 'final':finalname})
        # get rid of the temp file
        self.package_file = finalname
        self.remove(sparsename)

class LinuxManifest(ViewerManifest):
    build_data_json_platform = 'lnx'

    def is_packaging_viewer(self):
        super(LinuxManifest, self).is_packaging_viewer()
        return 'package' in self.args['actions']


    def do(self, *actions):
        super(LinuxManifest, self).do(*actions)
        self.package_finish() # Always finish the package.
        if 'package' in self.actions:
            # package_finish() was called by super.do() so just create the TAR.
            self.create_archive()
        return self.file_list
    
    def construct(self):
        import shutil
        shutil.rmtree("./packaged/app_settings/shaders", ignore_errors=True);
        super(LinuxManifest, self).construct()

        self.path("licenses-linux.txt","licenses.txt")

        pkgdir = os.path.join(self.args['build'], os.pardir, 'packages')
        relpkgdir = os.path.join(pkgdir, "lib", "release")
        debpkgdir = os.path.join(pkgdir, "lib", "debug")

        with self.prefix("linux_tools"):
            self.path("client-readme.txt","README-linux.txt")
            self.path("client-readme-voice.txt","README-linux-voice.txt")
            self.path("client-readme-joystick.txt","README-linux-joystick.txt")
            self.path("wrapper.sh",self.viewer_branding_id())
            with self.prefix(dst="etc"):
                self.path("handle_secondlifeprotocol.sh")
                self.path("register_secondlifeprotocol.sh")
                self.path("refresh_desktop_app_entry.sh")
                self.path("launch_url.sh")
            self.path("install.sh")

        # Create an appropriate gridargs.dat for this package, denoting required grid.
        # self.put_in_file(self.flags_list(), 'gridargs.dat')

        with self.prefix(dst="bin"):
            self.path("%s-bin"%self.viewer_branding_id(),"do-not-directly-run-%s-bin"%self.viewer_branding_id())

        # recurses, packaged again
        self.path("res-sdl")

        # Get the icons based on the channel type
        icon_path = self.icon_path()
        print "DEBUG: icon_path '%s'" % icon_path
        with self.prefix(src=icon_path) :
            self.path("viewer.png","viewer_icon.png")
            with self.prefix(dst="res-sdl") :
                self.path("viewer_256.BMP","viewer_icon.BMP")

        # plugins
        with self.prefix(src="../llplugin/slplugin", dst="bin/llplugin"):
            self.path("SLPlugin")

        with self.prefix(src="../plugins", dst="bin/llplugin"):
            self.path2basename("filepicker", "libbasic_plugin_filepicker.so")
            self.path("gstreamer010/libmedia_plugin_gstreamer010.so", "libmedia_plugin_gstreamer.so")
            self.path("cef/libmedia_plugin_cef.so", "libmedia_plugin_cef.so")
            self.path2basename("libvlc", "libmedia_plugin_libvlc.so")

        with self.prefix(src=os.path.join(pkgdir, 'lib', 'vlc', 'plugins'), dst="bin/llplugin/vlc/plugins"):
            self.path( "plugins.dat" )
            self.path( "*/*.so" )

        with self.prefix(src=os.path.join(pkgdir, 'lib' ), dst="lib"):
            self.path( "libvlc*.so*" )

        # CEF common files
        with self.prefix(src=os.path.join(pkgdir, 'resources'), dst=os.path.join('bin', 'llplugin') ):
            self.path("cef.pak")
            self.path("cef_extensions.pak")
            self.path("cef_100_percent.pak")
            self.path("cef_200_percent.pak")
            self.path("devtools_resources.pak")
            self.path("icudtl.dat")

        with self.prefix(src=os.path.join(pkgdir, 'resources', 'locales'), dst=os.path.join('bin', 'llplugin', 'locales') ):
            self.path("*.pak")
         
        with self.prefix(src=os.path.join(pkgdir, 'bin', 'release'), dst=os.path.join('bin', 'llplugin') ):
            self.path("dullahan_host")
            self.path("chrome-sandbox")
            self.path("natives_blob.bin")
            self.path("snapshot_blob.bin")
            self.path("v8_context_snapshot.bin")

        # plugin runtime
        with self.prefix(src=relpkgdir, dst=os.path.join('bin', 'llplugin') ):
            self.path("libcef.so")

        # llcommon
        if not self.path("../llcommon/libllcommon.so", "lib/libllcommon.so"):
            print "Skipping llcommon.so (assuming llcommon was linked statically)"

        self.path("featuretable_linux.txt")

    def package_finish(self):
        installer_name = self.installer_base_name()

        self.strip_binaries()

        # Fix access permissions
        self.run_command(['find', self.get_dst_prefix(),
                          '-type', 'd', '-exec', 'chmod', '755', '{}', ';'])
        for old, new in ('0700', '0755'), ('0500', '0555'), ('0600', '0644'), ('0400', '0444'):
            self.run_command(['find', self.get_dst_prefix(),
                              '-type', 'f', '-perm', old,
                              '-exec', 'chmod', new, '{}', ';'])
        self.package_file = installer_name + '.tar.xz'

    def create_archive(self):
        installer_name = self.installer_base_name()
        # temporarily move directory tree so that it has the right
        # name in the tarfile
        realname = self.get_dst_prefix()
        tempname = self.build_path_of(installer_name)
        self.run_command(["mv", realname, tempname])
        try:
            # only create tarball if it's a release build.
            if True:# self.args['buildtype'].lower() == 'release':
                # --numeric-owner hides the username of the builder for
                # security etc.
                self.run_command(['tar', '-C', self.get_build_prefix(),
                                  '--numeric-owner', '-cJf',
                                 tempname + '.tar.xz', installer_name])
            else:
                print "Skipping %s.tar.xz for non-Release build (%s)" % \
                      (installer_name, self.args['buildtype'])
        finally:
            self.run_command(["mv", tempname, realname])

    def strip_binaries(self):
        if self.args['buildtype'].lower() == 'release' and self.is_packaging_viewer():
            print "* Going strip-crazy on the packaged binaries, since this is a RELEASE build"
            # makes some small assumptions about our packaged dir structure
            try:
                self.run_command(
                    ["find"] +
                    [os.path.join(self.get_dst_prefix(), dir) for dir in ('bin', 'bin/llplugin', 'lib', 'lib32', 'lib64')] +
                    ['-executable', '-type', 'f', '!', '-name', 'update_install', '-exec', 'strip', '-S', '{}', ';'])
            except ManifestError as err:
                print err.message
                pass

class Linux_i686_Manifest(LinuxManifest):
    address_size = 32

    def construct(self):
        super(Linux_i686_Manifest, self).construct()

        pkgdir = os.path.join(self.args['build'], os.pardir, 'packages')
        relpkgdir = os.path.join(pkgdir, "lib", "release")
        debpkgdir = os.path.join(pkgdir, "lib", "debug")

        if (not self.standalone()) and self.prefix(src=relpkgdir, dst="lib"):
            self.path("libapr-1.so*")
            self.path("libaprutil-1.so*")
            self.path("libexpat.so.*")
            self.path("libGLOD.so")
            self.path("libSDL-1.2.so.*")
            self.path("libalut.so")
            self.path("libopenal.so.1")

            try:
                self.path("libtcmalloc_minimal.so.0")
                self.path("libtcmalloc_minimal.so.0.2.2")
                pass
            except:
                print "tcmalloc files not found, skipping"
                pass

            try:
                self.path("libfmod.so*")
                pass
            except:
                print "Skipping libfmod.so - not found"
                pass
            self.end_prefix()

        # Vivox runtimes
        with self.prefix(src=relpkgdir, dst="bin"):
            self.path("SLVoice")
        with self.prefix(src=relpkgdir, dst="lib"):
            self.path("libortp.so")
            self.path("libsndfile.so.1")
            self.path("libvivoxoal.so.1")
            self.path("libvivoxsdk.so")
            self.path("libvivoxplatform.so")

        # CEF renderers
        with self.prefix(src=os.path.join(relpkgdir, 'swiftshader'), dst=os.path.join("lib", "swiftshader") ):
            self.path( "*.so" )
        with self.prefix(src=relpkgdir, dst="lib"):
            self.path("libEGL.so")
            self.path("libGLESv2.so")


class Linux_x86_64_Manifest(LinuxManifest):
    address_size = 64

    def construct(self):
        super(Linux_x86_64_Manifest, self).construct()

        pkgdir = os.path.join(self.args['build'], os.pardir, 'packages')
        relpkgdir = os.path.join(pkgdir, "lib", "release")
        debpkgdir = os.path.join(pkgdir, "lib", "debug")

        if (not self.standalone()) and self.prefix(relpkgdir, dst="lib64"):
            self.path("libapr-1.so*")
            self.path("libaprutil-1.so*")
            self.path("libexpat.so*")
            self.path("libGLOD.so")
            self.path("libSDL-1.2.so*")
            self.path("libhunspell*.so*")
            self.path("libalut.so*")
            self.path("libopenal.so*")

            try:
                self.path("libtcmalloc.so*") #formerly called google perf tools
                pass
            except:
                print "tcmalloc files not found, skipping"
                pass

            try:
                self.path("libfmod.so*")
                pass
            except:
                print "Skipping libfmod.so - not found"
                pass

            self.end_prefix()

        # Vivox runtimes
        with self.prefix(src=relpkgdir, dst="bin"):
            self.path("SLVoice")
        with self.prefix(src=relpkgdir, dst="lib32"):
            self.path("libortp.so")
            self.path("libsndfile.so.1")
            self.path("libvivoxoal.so.1")
            self.path("libvivoxsdk.so")
            self.path("libvivoxplatform.so")

        # CEF renderers
        with self.prefix(src=os.path.join(relpkgdir, 'swiftshader'), dst=os.path.join("lib64", "swiftshader") ):
            self.path( "*.so" )
        with self.prefix(src=relpkgdir, dst="lib64"):
            self.path("libEGL.so")
            self.path("libGLESv2.so")


################################################################

if __name__ == "__main__":
    extra_arguments = [
        dict(name='bugsplat', description="""BugSplat database to which to post crashes,
             if BugSplat crash reporting is desired""", default=''),
        ]
    main(extra=extra_arguments)
