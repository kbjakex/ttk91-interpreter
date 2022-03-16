# ttk91-interpreter
A compiler-interpreter for the [TTK91 assembly language](https://www.cs.helsinki.fi/group/titokone/ttk91_ref_en.html).
See the "programs" directory for some examples.

## Goals
* **Helpful**. Clear, understandable error messages and warnings at both compile time and runtime.
* **Fast**. A bytecode instruction takes 1.85-2ns to execute on "fuksilaptop" 2021 (i5-1135G7 @ 2.40GHz)
* **Convenient**. Running a program should be very easy. A built-in benchmarking tool is provided for easy and reliable testing.
* **Robust**. Needs more testing, but the program should ideally be able to handle any kind of input and not go bananas :)
* **Conformant**. It is little use if it doesn't accurately model the language it was built for. Should be fully conformant to my knowledge, but requires more testing.

## Usage
Grab and unzip the [latest release](https://github.com/kbjakex/ttk91-interpreter/releases/tag/v0.0.1).
Type `ttkc <filename.k91> [option(s)]` to compile & execute a ttk91 file, such as [this one](https://github.com/kbjakex/ttk91-interpreter/blob/main/programs/is_prime.k91).
Available options:
* `-i`/`--benchmark-iterations=<integer>`: The interpreter has a built-in benchmarking system that works by running the entire bytecode program several times, measuring the total time elapsed for all of the iterations, and computing the average time per run. This option sets the number of iterations. As a rule of thumb, to minimize the effects of fluctuation, try to get the total time at least over 10 seconds and consider closing other applications.
* `-bio`/`--bench-io[=<true/1/false/0>]`: The speed at which the interpreter prints integers is probably not of interest, so while benchmarking (benchmark iterations > 1), all printing is suppressed by default. Use `-bio=1` to re-enable printing.
* `-d`/`--dry[=<true/1/false/0>]`: Compiles the file but does not interpret the bytecode. Useful for checking for syntax correctness without running. Note that while the code could be compiled to a binary format, and the word "compiling" might imply doing that, this does not actually produce an output file.
* `-ss`/`--stack-size=<integer>`: Sets the size of the stack for the program. Defaults to 1 MiB.

Run `ttkc --help` for an up-to-date list.

## Building
Note: There are also [pre-built releases](https://github.com/kbjakex/ttk91-interpreter/releases/tag/v0.0.1) available! There's probably little reason to bother compiling yourself unless you're toying with the codebase

Curiously, the project is currently lacking a build system. Not for any particular reason other than that I did not get around to adding one. Open `build_commands.txt`, and copy & run the command in a terminal in the project directory :)

As always, it *should* compile cleanly out of the box, but if not, please open an issue, or contact me on Discord (see below).

If you're on linux and get an error with the release build, try without `-flto` (and please still let me know!) 

Also a note: The project relies on the "labels as a value" extension supported by both GCC and Clang, but likely not by MSVC. Development was done using Clang, so recommendation would be to get Clang if you're building on Windows. Multiple ways to do that, but an exceptionally convenient way to get a quite up-to-date version of everything needed is to get them from [WinLibs](https://winlibs.com/): download the latest 64-bit UCRT zip, unzip to a directory of choice, add the directory to path and run the build command.

## Contributing
Contributions of any form welcome. Testing and bug reports would be especially helpful, but also any performance improvements or feature contributions. Feature requests are also welcome (open an issue).

## Bug reporting
If you find any questionable behavior or have any other questions, easiest way is to shoot me a message on Discord: jetp250#8243 (Please don't hesitate!)

Issues can also be reported on the [issue tracker](https://github.com/kbjakex/ttk91-interpreter/issues). If possible, try to include a way to reproduce the issue and any possibly relevant details, but even just knowing there's a bug is far better than nothing :)

## Useful links
[Summary of the TTK91 ISA](https://www.cs.helsinki.fi/group/titokone/ttk91_ref_en.html) (Instruction Set Architecture)

## License
Licensed under MIT in hopes of making this a base for a collaborative effort, even if I am not involved.
