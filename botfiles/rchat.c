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

["politicians", "politician"] = 7
{
	"People should realize that they can govern themselves without the need for elected representation.";
	"I have no need for elected representation, I govern myself.";
	"There is no need for leadership here";
	"Politicians serve their own needs, not those they represent.";
	"Do you really need someone telling you what you can or cannot do?";
	"When people are honest there is no need for law or elected representation.";
	"Elected representation is the basis for all manor of oppression.";
	ponder;
}

["no", "Nada", "nope", !"no place"] = 5
{
	"are you sure about that?";
	"You sound so sure of yourself";
	"Really?";
	"Don't be so certain";
}


//will never be called as it is a multi-player command...
["nigger", "nigr", "wop", "kyke", "sandnigger", "sand-nigger", "sand nigger", "beanpicker", "wetback", "raghead", "macaca", "monkey", "irish pig", "redskin", "red-skin", "red skin", "rag head", "camel jockey", "cml jky", "diaper head", "bean picker", "cml-jky", "camel-jockey", "ngr"] = 1
{
	"shut the hell up you ", vicious_insult,".";
	"bigot";
	"There is no place for bigotry in the Arena";
	"you are a ", vicious_insult, ", do us all a favor and die.";
	"All colors explode the same, ", vicious_insult, ".";
	"People are people, unfortunately you are an idiot, ", vicious_insult, ".";
	"This is an equal opportunity frag fest, bigots are not welcome.";
	immaturity01;
	vicious_insult;
	response_insult;
	curse;
	
}

["why", !"why not"] = 8
{
	"why not?";
	"because dmn_clown said!";
	"My ", counselor, " said so.";
	botnames, " said so.";
	confused_response;
	peeps, " said so.";
}

["what is happening"] = 1
{
	"dork";
	"HA HA, what a dweeb";
	"putz";
	"schmuck";
}

["ha", "LOL", "laugh", "funny"] = 6
{
	"I'm not laughing.";
	"What is so funny?";
	"That wasn't funny...";
	"Sorry, I forgot to laugh";
	immaturity01;
	"hehe";
	"That was amusing";
	"Ok, that was funny";
	response_insult;
}

["fishing", !"freak fishing"] = 3
{
	"My ", family_member, " was eaten by a blue whale last ", month, ".";
	"I like to use ", substance, " as bait, but only when it is ", weather, ".";
	"I'd swear that asian carp can speak ", language, ".";
	response_insult;
}

["humiliating", "shamed"] = 3
{
	"LOL!";
	"Hehe... good";
	"You deserve to be";
	"Merry x-mas!";
}

["touch me there"] = 1
{
	"That isn't what your ", family_member, " said last night.";
	"why not?";
	response_insult;
}

["why not"] = 6
{
	"because ", peeps, " said.";
	"because I said";
	"because dmn_clown said so!";
	"A ", profession, " said so.";
}

["yes", "sure", "certainly"] = 2
{
	confused_response;
	neutral;
	"You sound positive.";
	"You've convinced me.";
	"Ok";
	
}

["universal harmony"] = 1
{
	"Do your work, then step back... the only path to serenity.";
	"Practice not-doing, and everything will fall into place.";
	"If you want to be in accord with the Tao, just do your job and then step back.";
	"When there is no desire all things are at peace.";
	"The more you know, the less you understand.";
}

["Pat Robertson"] = 1
{
	"I hear he has a ", substance, " addiction.";
	"I hear that he was caught in bed with a ", animal, " after smoking ", substance, ".";
	"This isn't a chat room!";
	response_insult;
}

["Jesus"] = 1
{
	"Jesus hates me.";
	"Why do you need a human sacrifice to be saved, sounds canibalistic to me...";
	"Jesus loves you, but I don't.";
	"I worship ", peeps, ".";
	"I bow my head to the holy ", animal, " but only after smoking ", substance, ".";
}

["addiction", "addicted to"] = 4
{
	"I am addicted to ", peeps, ".";
	"My ", animal, " likes to huff ", food, ".";
	"Drugs like ", substance, " rot your brain.";
	"That can be tough on the people around you.";
}

[(0, " is a bitch") &name, !"Grism", !"Gargoyle"] = 4
{
	"That is Mizz Bitch to you, ", fighter, ".";
	"I'm only rude to ", fighter, "s like you.";
	"You should see me out of the Arena...";
	"That isn't what your ", family_member, " said last night";
}

["anarchy"] = 3
{
	"Yo soy anarquista";
	"Yo soy un anticristo";
	"Yo quiero se anarquia";
	"Quiero desplazar";
	"While there is a lower class I am in it"; 
	"while there is a criminal element I am of it"; 
	"while there is a soul in prison, I am not free";
	"I have no country to fight for, my country is the earth, and I am a citizen of the world.";
	"The most heroic word in all languages is revolution.";
	"Intelligent discontent is the mainspring of civilization. Progress is born of agitation. It is agitation or stagnation.";
	"How pseudo revolutionary of you...";
}