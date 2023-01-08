# cellular-automaton-sequencer

Sequencer that is driven by a real-time generation of a 1D totalistic cellular automaton. The automaton has 3 colors and random rules are being generated on every button click. The pattern is shown on the OLED screen. The code works for the Bela board, with the sequencer in Pure Data (see `_main.pd`) and the automaton generation, button and screen control with C++ (see `render.cpp`, the coments denoted by `AUTOMATON` keyword).



https://user-images.githubusercontent.com/49707233/211184285-44235bbf-f0bd-421e-95ef-a359ba58fdbe.mp4

