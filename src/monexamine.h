#pragma once
#ifndef MONEXAMINE
#define MONEXAMINE

class monster;

namespace monexamine
{
/*
*Manages the milking and milking cool down of monsters.
*Milked item uses starting_ammo, where ammo type is the milked item 
*and ammount the times per day you can milk the monster.
*/
void milk_source( monster &source_mon );
}
#endif
