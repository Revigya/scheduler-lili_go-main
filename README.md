# scheduler-lili_go-main
This project builds on the original egos-2000 operating system (licensed under MIT) and modifies its core scheduling and process management mechanisms to meet advanced operating systems project requirements. All original license and author credits are retained in the source files. All modifications are also released under the MIT License.


## Overview
The original round-robin scheduler has been replaced with a multi-queue priority scheduler that incorporates aging to ensure high-priority tasks are served first while preventing starvation of lower-priority tasks. Several new system components and user utilities were added to support dynamic process control and testing.

## Features Implemented
- Multi-queue priority scheduler with aging mechanism
- SYS_SETPRIO system call for runtime priority adjustment
- Process control block extensions: priority, current aged priority, context switch count
- Default priorities established: kernel = 1, user = 2
- Additional user applications: kill, ps (with priority and context switch display), test_pri, and priority utilities
- File system and directory operations maintained and integrated
- User-level threading implementation added for concurrency

## Project Status
This project was completed as part of a university operating systems course.
All requirements were fully implemented. 

## License
This project is based on the original egos-2000 OS code, which is licensed under the MIT License.
All modifications and new files are also licensed under the MIT License.
See the LICENSE file for details.
