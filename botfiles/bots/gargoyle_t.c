/*
===========================================================================
Copyright (C) 2006 Dmn_clown (aka: Bob Isaac (rjisaac@gmail.com))

This file is part of Open Arena and is based upon Mr. Elusive's fuzzy logic
system found in Quake 3 Arena.

Open Arena is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Open Arena is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/


chat "gargoyle"
{
	#include "teamplay.h"

	type "game_enter"
	{
		"I hope that you are all ready to die, because you will.";
	}

	type "game_exit"
	{
		"~It's been real and ~it's been fun...";
	}

	type "level_start"
	{
		4, "was a great place before they screwed it up.";
	}

	type "level_end"
	{
		"Yawn... thought it would never end.";
	}

	type "level_end_victory"
	{
		"As usual... ~You still suck.";
	}

	type "level_end_lose"
	{
		"Ya Basta!";
	}

	type "hit_talking"
	{
		"Look asshole I was trying to hold a conversation!";
	}

	type "hit_nodeath"
	{
		TAUNT0;
		TAUNT1;
		TAUNT2;
	}

	type "hit_nokill"
	{
		HIT_NOKILL0;
		HIT_NOKILL1;
		HIT_NOKILL1;
	}

	type "death_telefrag"
	{
		DEATH_TELEFRAGGED0;
		DEATH_TELEFRAGGED0;
		DEATH_TELEFRAGGED0;
	}

	type "death_cratered"
	{
		DEATH_FALLING0;
		DEATH_FALLING0;
		DEATH_FALLING0;
	}

	type "death_lava"
	{
		"Anyone got any marshmallows?";
	}

	type "death_slime"
	{
		"Well it looked safer than that...";
	}

	type "death_drown"
	{
		"Ahh... sweet oblivion.";
	}

	type "death_suicide"
	{
		DEATH_SUICIDE0;
		DEATH_SUICIDE1;
		DEATH_SUICIDE2;
	}

	type "death_gauntlet"
	{
		DEATH_GAUNTLET0;
		DEATH_GAUNTLET1;
		DEATH_GAUNTLET1;
	}

	type "death_rail"
	{
		DEATH_RAIL1;
		DEATH_RAIL1;
		DEATH_RAIL0;
		 0, ", I invented the railgun!";
	}

	type "death_bfg"
	{
		DEATH_BFG1;
		 0, ", I invented the BFG!";
	}

	type "death_insult"
	{
		"took you long enough...";
		"Is that all you've got?";
		curse;
	}

	type "death_praise"
	{
		 "You are better than I thought, ", 0, ".";
	}

	type "kill_rail"
	{
		DEATH_RAIL1;
		DEATH_RAIL0;
		DEATH_RAIL1;
	}

	type "kill_gauntlet"
	{
		KILL_GAUNTLET0;
		KILL_GAUNTLET1;
		KILL_GAUNTLET0;
	}

	type "kill_telefrag"
	{
		TELEFRAGGED0;
		TELEFRAGGED1;
		
	}

	type "kill_suicide"
	{
		
		TAUNT1;
		TAUNT0;
	}

	type "kill_insult"
	{
		"Pathetic";
		"looser";
		KILL_EXTREME_INSULT;
		curse;
	}

	type "kill_praise"
	{
		D_PRAISE0;
		D_PRAISE1;
		D_PRAISE3;
	}

	type "random_insult"
	{
		 "I've seen bacteria with a better chance of getting a frag than ~you, ", 0, ".";
	}

	type "random_misc"
	{
		"Ever have an itch that you just can't scratch?";
		"You know, I really do want to be anarchy... how do I go about that?";
	}
}
