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

	//the bot doesn't know who someone is
type "whois"
{
"Ok, so who is ", 0, "?";
"Who in their right mind uses the name ", 0, "?";
"Who in the hell is ", 0, "?";
"Is ", 0, " a friend of yours?";
"Who the bloodyhell is ",0," .";
0, "!?! Who dat?";
"How can I kill ", 0, " when I haven't the foggiest idea who ", 0, " is?";

}

	//the bot doesn't know where someone is hanging out
type "whereis"
{
"So where is ", 0, "?";
"Ok, so where is", 0, ".";
"Would someone please tell me where ", 0, " is.";
" ", 0, " hanging out?";
"Where the hell is ", 0, "?";
"Since when am I ", 0, "'s keeper?";
} 

	//the bot asks where you are
type "whereareyou"
{
"Yo, where are you ", 0, "?";
"Hello!! ", 0, "Where are you hiding?";
"I can't find you ", 0, ". Where are you?";
"Where did you scuttle off to ", 0, "?";
"How am I supposed to find you, ", 0, "?";
}

//cannot find something
type "cannotfind"
{
"Where would that be ", 0, "?";
"Where the hell is a ", 0, "?";
"Where is a ", 0, " in this level?";
"Is there a, ", 0, " in this level? I sure can't find it, I must be blind.";
} 

	//bot tells where he/she is
type "location"
{
"By the ", 0," what are you blind?";
"I am at the ", 0;
} 

//bot tells where he/she is and near which base
type "teamlocation"
{
"I'm near the ", 0, " in the ", 1, "base.";
"By the ", 0, " in the ", 1, " base.";
} 

	//start helping
type "help_start"
{
"I'm almost there, ", 0, ", coming to help.";
"Ok, I'm on my way,", 0,".";
"Keep 'em busy, ", 0," I'm on my way.";
}

	//start accompanying
type "accompany_start"
{
"Consider me your shadow ", 0 ,".";
affirmative, "... what else have I got planned?";

}

	//stop accompanying
type "accompany_stop"
{
"It's been real, but I'm off on my own, ", 0,".";
}

	//cannot find companion
type "accompany_cannotfind"
{
"Where the hell are you ", 0, "?";
"Where are you hiding ", 0, "?";

} 

	//arrived at companion
type "accompany_arrive"
{
"At your disposal ", 0, ".";

}
	//bot decides to accompany flag or skull carrier
type "accompany_flagcarrier"
{
"I've got your back ", 0, ".";
}

	//start defending a key area
type "defend_start"
{
"I'll defend the ", 0, ".";


}

	//stop defending a key area
type "defend_stop"
{
"That's it, I'll stop defending the ", 0, ".";

}

	//start getting an item
type "getitem_start"
{
"I'll get the ", 0, ".";

}
	//item is not there
type "getitem_notthere"
{
"the ", 0, " ain't there";
}
	//picked up the item
type "getitem_gotit"
{
"I got the ", 0, "!";

}

	//go kill someone
type "kill_start"
{
"I'm going to kill ", 0,", wish me luck.";
0, " will be toast.";
0, " is a goner.";
0, " will be given a pair of cement shoes.";
"Ok";
"What ever you say";
"Finally some fun!";
}
	//killed the person
type "kill_done"
{
"Well that was easy. ", 0, " is dead.";
0, " has been wacked.";
0, " was given cement shoes.";
0, " kicked the bucket.";
0, " was taken out.";
0, " just bought the farm.";
0, " is now just dust in the wind.";
}

	//start camping
type "camp_start"
{
"This is a good sniping position.";
}

	//stop camping
type "camp_stop"
{
"Time to move on.";
}

	//in camp position
type "camp_arrive" //0 = one that ordered the bot to camp
{
"I'm there. ", 0, ", time to snipe.";
}

	//start patrolling
type "patrol_start" //0 = locations
{
"I'll patrol from here, ", 0, ".";
}

	//stop patrolling
type "patrol_stop"
{
"The patrol is over, nothing to report.";
}

	//start trying to capture the enemy flag
type "captureflag_start"
{
"Their flag will be ours.";
}

	//return the flag
type "returnflag_start"
{
"I'll get our flag back.";
}

	//attack enemy base
type "attackenemybase_start"
{
"Got it, their base is toast.";
}

	//harvest
type "harvest_start"
{
"Death to the infidels!  Their skulls will be mine!";
"Death to the infidels!";
"The infidels shall die!";
}

	//bot is dismissed
type "dismissed"
{
"A'ight, I'm off.";
}

	//the bot joined a sub-team
type "joinedteam"
{
"Ok, I'm on the ", 0, " team now.";
}

	//bot leaves a sub team
type "leftteam" //0 = team name
{
"I just left the ", 0, " team.";
}

	//bot is in a team
type "inteam"
{
"Hey, I'm on the ", 0, " team now.";

}

	//bot is in no team
type "noteam"
{
"I work better alone.";
"No one wanted me on their team :-(";
"I have no friends. My ", counselor, " tells me this is bad.";
}

	//the checkpoint is invalid
type "checkpoint_invalid"
{
"That checkpoint does not exist.";
}

	//confirm the checkpoint
type "checkpoint_confirm" //0 = name, 1 = gps
{
affirmative, " Yep ", 0, " at ", 1, " is there.";
}

	//follow me
type "followme"
{
"What the hell are you waiting for ", 0, "? Get over here!";
}

	//stop leading
type "lead_stop"
{
"That's it find someone else who wants the responsibility.";
}

	//the bot is helping someone
type "helping"
{
"I'm trying to help, ", 0, ".";
}

	//the bot is accompanying someone
type "accompanying"
{
"I'm shadowing ", 0, ".  Is that alright?";
}

	//the bot is defending something
type "defending"
{
"I'm defending ", 0, ".";
}

	//the bot is going for an item
type "gettingitem"
{
"I'm off to get the ", 0, ".";
}

	//trying to kill someone
type "killing"
{
"I've been trying to kill ", 0, ".";
}

	//the bot is camping
type "camping"
{
"Toasting marshmallows and sniping scum.";
"Where I am supposed to be, camping.";
}

	//the bot is patrolling
type "patrolling"
{
"On patrol, can't talk now.";
}

	//the bot is capturing the flag
type "capturingflag"
{
"Gots to get the flag.";
}

	//the bot is rushing to the base
type "rushingbase"
{
"Rushing to the base.";
}

	//trying to return the flag
type "returningflag"
{
"Getting the flag.";
}

type "attackingenemybase"
{
"I'm destroying their base!  Care to help?";
}

type "harvesting"
{
"Collecting skulls, what are you doing?";
}

	//the bot is just roaming a bit
type "roaming"
{
"Rambling around, fragging at whim.";
"Mindlessly roaming around, like I was told.";
"~Wacking fools piece-meal";
}

type "wantoffence"
{
"Let me go on offense.";
"Can I be on offense?";
}

type "wantdefence"
{
"I think I can handle the big D.";
"Can I be on defense?";
}

	//the bot will keep the team preference in mind
type "keepinmind"
{
"A'ight, ", 0," I'll keep it in mind.";

}

	//==========================
	// teamplay chats
	//==========================
	//team mate killed the bot
type "death_teammate"
{
"Same team, dumbass!";
"Hey ", 0," I'm on your team... idiot!";
}
	//killed by a team mate
type "kill_teammate"
{
"hehe... oops.";
"Sorry!";
}

	//==========================
	// CTF useless chats
	//==========================

	//team mate got the enemy flag
type "ctf_gotflag"
{
"It's about time, ", 0, " now get that flag home!";
}
	//team mate gets the enemy flag to the base
type "ctf_captureflag"
{
"Sweet, gj, ", 0, ".";
}
	//team mate returns the base flag
type "ctf_returnflag"
{
"Nice assist, ", 0, ".";
} //end type
	//team mate defends the base
type "ctf_defendbase"
{
"Nice D work there, ", 0, ".";
}
	//team mate carrying the enemy flag dies
type "ctf_flagcarrierdeath"
{
"Go get our flag!";
}
	//team mate kills enemy with base flag
type "ctf_flagcarrierkill"
{
"Yo, ", 0," get our flag now!";
}

	//==========================
	// NOTE: make sure these work with match.c
	//==========================
	//ask who the team leader is
type "whoisteamleader"
{
"Who's the leader?";
}

	//I am the team leader
type "iamteamleader"
{
"I am the leader.";
}
	//defend the base command
type "cmd_defendbase"
{
0, " defend our base.";
}
	//get the enemy flag command
type "cmd_getflag"
{
"Yo, ", 0, " get the flag!";
0, " get their flag.";
0, " get the flag.";
}
	//accompany someone command
type "cmd_accompany"
{
"Hey, ", 0, " shadow ", 1;

}
	//accompany me command
type "cmd_accompanyme"
{
0, " you should follow me.";
}
	//attack enemy base command
type "cmd_attackenemybase"
{
0, " go after their base";
}
	//return the flag command
type "cmd_returnflag"
{
0, " return our flag, dammit!";
0, " get our flag back, ASAP!";
}
	//go harvesting
type "cmd_harvest"
{
0, ", you should collect some skulls.";
}


