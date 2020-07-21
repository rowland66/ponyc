# Installing Pony

Prebuilt Pony binaries are available on a number of platforms. They are built using a very generic CPU instruction set and as such, will not provide maximum performance. If you need to get the best performance possible from your Pony program, we strongly recommend [building from source](BUILD.md).

Prebuilt Pony installations will use clang as the default C compiler and clang++ as the default C++ compiler. If you prefer to use different compilers, such as gcc and g++, these defaults can be overridden by setting the `$CC` and `$CXX` environment variables to your compiler of choice.

## FreeBSD 12.1

Prebuilt FreeBSD 12.1 nightly packages are available for download from our [Cloudsmith repository](https://cloudsmith.io/~ponylang/repos/nightlies/packages/?q=name%3A%27%5Eponyc-x86-64-unknown-freebsd-12.1.tar.gz%24%27).

Starting with the next ponyc release, you'll also be able to download prebuilt ponyc releases from Cloudsmith. We'll update this page with the link once they become available.

## Linux

Prebuilt Linux packages are available via [ponyup](https://github.com/ponylang/ponyup) for Glibc and musl libc based Linux distribution. You can install nightly builds as well as official releases using ponyup.

To install the most recent ponyc:

```bash
ponyup update ponyc release
```

Selecting the correct distro:

By default, when you use ponyup on a Glibc based Linux distro, it will install a version of ponyc that was built on the most recent Ubuntu LTS release. If your distro uses the same Glibc version, then everything will work fine.

If you get an error like: `ponyc: /lib/x86_64-linux-gnu/libm.so.6: version `GLIBC_2.29' not found (required by ponyc)` then your version of Glibc is not compatible.

You can use `ponyup show ponyc` to see all available distributions. Select the one that matches your distro using the `ponyup default` command. For example:

```
ponyup show ponyc


If you are using a Glibc based Linux distro, then you might need to select an specific distribution package rather than
Additional requirements:

All ponyc Linux installations need to have a C compiler such as clang installed. Compilers other than clang might work, but clang is the officially supported C compiler. The following distributions have additional requirements:

Distribution | Requires
--- | ---
alpine | libexecinfo
fedora | libatomic

## macOS

Prebuilt macOS packages are available via [ponyup](https://github.com/ponylang/ponyup). You can also install nightly builds using ponyup.

To install the most recent ponyc on macOS:

```bash
ponyup update ponyc release
```

## Windows

Windows users will need to install:

- Visual Studio 2019 or 2017 (available [here](https://www.visualstudio.com/vs/community/)) or the Visual C++ Build Tools 2019 or 2017 (available [here](https://visualstudio.microsoft.com/visual-cpp-build-tools/)).
  - If using Visual Studio, install the `Desktop Development with C++` workload.
  - If using Visual C++ Build Tools, install the `Visual C++ build tools` workload, and the `C++ core features` individual component.
  - Install the latest `Windows 10 SDK (10.x.x.x) for Desktop` component.

Once you have installed the prerequisites, you can download the latest ponyc release from [Cloudsmith](https://dl.cloudsmith.io/public/ponylang/releases/raw/versions/latest/ponyc-x86-64-pc-windows-msvc.zip).

Unzip the release file in a convenient location, and you will find `ponyc.exe` in the `bin` directory. Following extraction, to make `ponyc.exe` globally available, add it to your `PATH` either by using Advanced System Settings->Environment Variables to extend `PATH` or by using the `setx` command, e.g. `setx PATH "%PATH%;<directory you unzipped to>\bin"`
