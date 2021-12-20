# ttk91-interpreter
A compiler-interpreter for the TTK91 assembly language developed at Helsinki University (not by me).

## Goals
* **Helpful**. Clear, understandable error messages and warnings at both compile time and runtime.
* **Fast**. A bytecode instruction takes 1.85-2ns to execute on "fuksilaptop" 2021 (i5-1135G7 @ 2.40GHz)
* **Convenient**. Running a program should be very easy. A built-in benchmarking tool is provided for easy and reliable testing.
* **Robust**. Needs more testing, but the program should ideally be able to handle any kind of input and not go bananas :)
* **Conformant**. It is little use if it doesn't accurately model the language it was built for. Should be fully conformant to my knowledge, but requires more testing.

## Usage
Type `ttki <filename.k91> [option(s)]` to compile & run a file.
Available options
* `-i`/`--benchmark-iterations=<integer>`: The interpreter has a built-in benchmarking system that works by running the entire bytecode program several times, measuring the total time elapsed for all of the iterations, and computing the average time per run. This option sets the number of iterations. As a rule of thumb, to minimize the effects of fluctuation, try to get the total time at least over 10 seconds and consider closing other applications.
* `-bio`/`--bench-io[=<true/1/false/0>]`: The speed at which the interpreter prints integers is probably not of interest, so while benchmarking (benchmark iterations > 1), all printing is suppressed by default. Use `-bio=1` to re-enable printing.
* `-d`/`--dry[=<true/1/false/0>]`: Compiles the file but does not interpret the bytecode.
* `-ss`/`--stack-size=<integer>`: Sets the size of the stack for the program. Defaults to 1 MiB.

Run `ttki --help` for an up-to-date list.

## Building
Curiously, the project is currently lacking a build system. Not for any particular reason other than that I did not get around to adding one. Open `build_commands.txt`, and copy & run the command in a terminal in the project directory :)

As always, it *should* compile cleanly out of the box, but if not, please open an issue.
If you're on linux and get an error with the release build, try without `-flto` (and please still open an issue).

Also a note: The project relies on the "labels as a value" extension supported by both GCC and Clang, but likely not by MSVC. Development was done using Clang, so recommendation would be to get Clang if you're building on Windows. Multiple ways to do that, but an exceptionally convenient way to get a quite up-to-date version of everything needed is to get them from [WinLibs](https://winlibs.com/): download the latest 64-bit UCRT zip, unzip to a directory of choice, add the directory to path and run the build command.

## Contributing
Contributions of any form welcome. Testing and bug reports would be especially helpful, but also any performance improvements or feature contributions. Feature requests are also welcome (open an issue).

## Useful links
[Summary of the TTK91 ISA](https://www.cs.helsinki.fi/group/titokone/ttk91_ref_en.html)

## License
Licensed under MIT in hopes of making this a base for a collaborative effort, even if I am not involved.
