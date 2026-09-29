// SPDX-License-Identifier: MIT
#include "tab5_keyboard_map.h"
#include <cassert>
#include <cstdio>
using namespace tab5_keyboard;
static uint8_t event(unsigned row,unsigned col,bool pressed=true){return (pressed?0x80:0)|(row<<4)|col;}
int main(){
    Decoder d;
    assert(!d.feed(0xff) && !d.feed(0xff) && !d.feed(event(7,0)));
    assert(d.feed(event(2,1)) && d.key=='q' && d.pressed);
    assert(!d.feed(event(2,1))); // duplicate key-down must not add text
    assert(d.feed(event(2,1,false)) && !d.pressed);
    d.feed(event(3,1));d.feed(event(2,1));assert(d.key=='Q');
    d.feed(event(3,1,false));d.feed(event(2,1,false));assert(d.key=='Q' && !d.pressed);
    d.feed(event(3,0));d.feed(event(1,13));assert(d.key=='|');
    d.feed(event(1,13,false));d.feed(event(2,12));assert(d.key=='"');
    d.feed(event(2,12,false));d.feed(event(3,12));assert(d.key=='=');
    d.reset();d.feed(event(3,1));d.feed(event(0,1));assert(d.key=='!');
    d.feed(event(0,1,false));d.feed(event(2,0));assert(d.key==Prev);
    d.reset();d.feed(event(2,0));assert(d.key==Next);
    d.reset();d.feed(event(4,0));assert(!d.feed(event(3,2))); // ctrl+a must not type 'a'
    d.reset();d.feed(event(4,1));assert(!d.feed(event(3,2)));
    d.reset();assert(!d.pressed && !d.key);d.feed(event(2,13));assert(d.key==Backspace);
    d.reset();d.feed(event(3,13));assert(d.key==Enter);
    d.reset();d.feed(event(4,10));assert(d.key==Left);
    // All matrix positions and malformed bytes must be safe after arbitrary events.
    for(int i=0;i<100000;i++)d.feed((uint8_t)(i*139));
    puts("Tab5Keyboard: matrix, modifiers, symbols, focus keys, releases and malformed input passed");
}
