<img src="img/11mm.png" width=100 height=auto />

*11mm* is a proof verifier for the [Metamath](https://us.metamath.org) language, written in C++11.

It currently verifies `set.mm` without compression in 12.77s. To check this yourself, issue these commands:
```
$ metamath
MM> read set.mm
MM> save proof * /normal
(mm message here) Q <enter>
MM> write source set_normal.mm
MM> quit
$ 11mm set_normal.mm
```
It should blast your screen with a bunch of `[OK]`'s.

## Todo:
 - [ ] LaTeX and HTML conversion (`$t`);
 - [ ] Compressed proof format (it's so over).
