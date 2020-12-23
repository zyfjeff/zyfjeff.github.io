## Chapter1

1. ten 64-bit registers.
2. extended BPF version up to four times faster than the original BPF implementation.
3. The BPF verifier, added these required safety guarantees. It ensures that any BPF program will complete without crashing, 
    and it ensures that programs don’t try to access memory out of range. These advantages come with certain restrictions, 
    though: programs have a maximum size allowed, and loops need to be bounded to ensure that the system’s memory is never exhausted by a bad BPF program.
4. BPF maps will become the main mechanism to exchange data between the kernel and user-space

