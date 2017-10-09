#!/usr/bin/env python
# Dependency Tracker for C++ source tree
# Warning: This program do not understand any preprocessors and macros.
# And this program cannot track the dependendy if the header's name and source's name are different.
# (net_util.h <-\-> net_util_posix.cc)
#
# You may use `exclude` option to leave out unwanted dependency.
from __future__ import print_function

import os
import re
import datetime
import argparse
import collections
import subprocess
import shutil
from pprint import pprint


class DependencyTree(object):
    def __init__(self, excludes, incdirs, exts, debug=False):
        self.incdirs = (["."] + incdirs) if incdirs else ["."]
        self.excludes = set(excludes) if excludes else set()
        self.excludes_used = set()
        self.files_not_found = set()
        self.debug = debug
        self.wherefrom = {}
        self.depmap = {}
        self.vis = set()
        self.exts = exts

    def scan_from(self, target):
        exts = self.exts
        depmap = self.depmap
        q = []
        vis = self.vis

        def enq(new, now):
            if not new:
                if self.debug:
                    print("invalid file in q from " + now)

            if not os.path.exists(new):
                tmp = new
                new = self.realpath(new, now)
                if not new:
                    self.files_not_found.add(tmp)
                    if self.debug:
                        print("resolve to invalid file: " + tmp + " from " + now)
                    return
    
            if new in self.excludes:
                self.excludes_used.add(new)
                return

            if new not in vis:
                vis.add(new)
                q.append(new)
                depmap[new] = []
                self.wherefrom[new] = now

        enq(target, None)

        while q:
            now = q.pop(0)

            if not now:
                if self.debug:
                    raise IOError("invalid file in queue")

            if self.debug:
                n = now
                print("  [ ", end=" ")
                while n:
                    print(n, " => ", end=" ")
                    n = self.wherefrom[n]
                print(" ]")

            for ext in exts:
                if now.endswith(".h") and os.path.exists(now[:-2] + ext):
                    enq(now[:-2] + ext, now)
                    depmap[now].append(now[:-2] + ext)

            for dependency in self.parse_cc(now):
                enq(dependency, now)
                # print now, dependency
                depmap[now].append(dependency)

        return depmap

    def show_files_not_found(self):
        if len(self.files_not_found) > 0:
            print("these files not found:")
            pprint(self.files_not_found)

    def find_chromium_root(self):
        for d in self.incdirs:
            if 'chromium' in d:
                return d
        return None

    def write_feature_header(self, path2write, pp_symbol):
        f = open(path2write, "w")
        f.write("#pragma once" + os.linesep)
        f.write("#define " + pp_symbol + os.linesep)
        f.write("#include \"interop/compile_flags.h\"" + os.linesep)
        f.write("#undef " + pp_symbol + os.linesep)
        f.write("#include \"build/buildflag.h\"" + os.linesep)
        f.close()
        return path2write

    def write_build_date_header(self, path2write):
        f = open(path2write, "w")
        build_date = '{:%b %d %Y %H:%M:%S}'.format(datetime.datetime.utcnow())
        output = ('// Generated by //tools/inject.py\n'
           '#ifndef BUILD_DATE\n'
           '#define BUILD_DATE "{}"\n'
           '#endif // BUILD_DATE\n'.format(build_date))
        f.write(output)
        f.close()
        return path2write

    def run(self, command):
        retcode = subprocess.call(command, shell=True)
        if retcode != 0:
            raise OSError("command '{}' fails with code {}".format(command, retcode))

    def write_effective_tld_cc(self, path2write):
        src = path2write.replace("-inc.cc", ".gperf")
        self.run("python {}/net/tools/dafsa/make_dafsa.py {} {}".format(self.find_chromium_root(), src, path2write))
        return path2write

    def write_log_list_cc(self, path2write):
        src = path2write.replace("-inc.cc", ".json")
        self.run("python {}/net/tools/ct_log_list/make_ct_known_logs_list.py {} {}".format(self.find_chromium_root(), src, path2write))
        return path2write

    def find_src_java_path(self, jni_header_path):
        for pre in ["base", "net"]:
            src_java = jni_header_path.replace("_jni.h", ".java").replace("jni/", "{}/android/java/src/org/chromium/{}/".format(pre, pre))
            print("try src_java = " + src_java)
            if os.path.exists(src_java):
                return src_java
        return None

    def generate_jni_headers(self, path2write):
        root = self.find_chromium_root()
        src_java = self.find_src_java_path(path2write)
        if not src_java:
            print("src_java not found for " + path2write)
            return src_java

        if not os.path.exists(os.path.dirname(path2write)):
            os.makedirs(os.path.dirname(path2write))

        self.run("python {}/base/android/jni_generator/jni_generator.py --input_file={} --includes={} --output_dir={}/jni".format(
            root, 
            src_java, 
            ",".join(["base/android/jni_android.h", "base/android/jni_generator/jni_generator_helper.h"]),
            root
        ))
        return path2write

    def make_pp_symbol(self, now):
        return "_".join(os.path.dirname(now).split(os.sep)).upper() + "_COMPILE_FLAGS"

    def rescue_file_missing(self, non_real_path, imported_from_rpath):
        if not os.path.dirname(non_real_path):
            rescue_path = os.path.dirname(imported_from_rpath) + "/" + non_real_path
            if os.path.exists(rescue_path):
                return rescue_path
        if non_real_path.endswith("features.h"):
            root = self.find_chromium_root()
            return self.write_feature_header(os.path.join(root, non_real_path), self.make_pp_symbol(non_real_path))
        elif "debugging_flags.h" in non_real_path:
            root = self.find_chromium_root()
            return self.write_feature_header(os.path.join(root, non_real_path), "BASE_DEBUG")
        elif "generated_build_date.h" in non_real_path:
            root = self.find_chromium_root()
            return self.write_build_date_header(os.path.join(root, non_real_path))
        elif "effective_tld_names" in non_real_path:
            root = self.find_chromium_root()
            return self.write_effective_tld_cc(os.path.join(root, non_real_path))            
        elif "certificate_transparency/log_list" in non_real_path:
            root = self.find_chromium_root()
            return self.write_log_list_cc(os.path.join(root, non_real_path))
        elif non_real_path.startswith("jni/"):
            root = self.find_chromium_root()
            return self.generate_jni_headers(os.path.join(root, non_real_path))
        else:
            return None

    def realpath(self, path, imported_from_rpath):
        for d in self.incdirs:
            rpath = os.path.join(d, path)
            if os.path.exists(rpath):
                return rpath 
        return self.rescue_file_missing(path, imported_from_rpath)

    def parse_cc(self, source):
        r = []
        f = None
        try:
            f = open(source, "r")
            for line in f:
                line = line.strip()
                m = re.search('^\s*#\s*include\s+"(.+)"', line)
                if not m:
                    if (re.search('^\s*#\s*include', line)) and (not re.search('<(.+)>', line)):
                        print("has include but not header:" + line)
                    continue
                else:
                    fname = m.group(1)
                    rfname = self.realpath(fname, source)
                    if not rfname:
                        if self.debug:
                            print("rfname not found:" + fname)
                        self.files_not_found.add(fname)
                        continue
                    r.append(rfname)
        finally:
            if f:
                f.close
        return r

    def traverse(self, lb):
        for k in self.depmap.keys():
            lb(k)

def load_file_list(filelist, file):
    if not filelist:
        filelist = set()

    with open(file, "r") as f:
        for line in f:
            l = line.strip()
            if l.startswith("#"):
                continue
            elif len(l) <= 0:
                continue
            else:
                filelist.add(l)

    if len(filelist) <= 0:
        raise IOError("invalid exclude file")

    return filelist


class CopyFilesVisitor(object):
    def __init__(self, chromium_root, outdir):
        self.chromium_root = chromium_root
        self.outdir = outdir

    def visit(self, file):
        if file.startswith(self.chromium_root):
            relpath = file.replace(self.chromium_root, "")
            dest = os.path.join(self.outdir, relpath[1:])
            #print("copy " + file + " => " + dest)
            if not os.path.exists(os.path.dirname(dest)):
                os.makedirs(os.path.dirname(dest))
            shutil.copyfile(file, dest)
    def syncdir(self, srcdir):
        if srcdir.startswith(self.chromium_root):
            relpath = srcdir.replace(self.chromium_root, "")
            dest = os.path.join(self.outdir, relpath[1:])
            #print("copy " + file + " => " + dest)
            if os.path.exists(dest):
                shutil.rmtree(dest)
            shutil.copytree(srcdir, dest, ignore=shutil.ignore_patterns('*.git*', '*.deps*', '*.libs*', '*.lo', '*.o', '*unittest*', '*_test*', 'test*'))

def main():
    parser = argparse.ArgumentParser(description="C++ Dependency Tracker and Copier for chromium src")
    parser.add_argument("srcroot", type=str)
    parser.add_argument("--altroot", action="append")
    parser.add_argument("--altroot_file", type=str)
    parser.add_argument("--exclude", action="append")
    parser.add_argument("--exclude_file", type=str)
    parser.add_argument("--debug", action="store_true")
    parser.add_argument("--dir", action="append")
    parser.add_argument("--sync_dir", action="append")
    parser.add_argument("--outdir", type=str)

    args = parser.parse_args()
    if args.exclude_file: 
        args.exclude = load_file_list(args.exclude, args.exclude_file)
    if args.altroot_file:
        args.altroot = load_file_list(args.altroot, args.altroot_file)

    exts = [".mm", "_posix.cc", "_linux.cc", "_win.cc", "_mac.cc", "_mac.mm", "_android.cc", "_fuchsia.cc", "_wrapper.cc", "_constants.cc", ".cpp", ".cc"]
    tree = DependencyTree(args.exclude, args.dir, exts, args.debug)
    tree.scan_from(args.srcroot)
    if args.altroot:
        for root in args.altroot:
            tree.scan_from(root)

    tree.show_files_not_found()

    visitor = CopyFilesVisitor(tree.find_chromium_root(), args.outdir)
    tree.traverse(visitor.visit)

    if args.sync_dir:
        for srcdir in args.sync_dir:
            visitor.syncdir(srcdir)

if __name__ == "__main__":
    main()