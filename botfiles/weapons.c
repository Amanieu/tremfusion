#include "inv.h"

#define IMPACT_DAMAGE			1		//straight impact damage
#define SPLASH_DAMAGE			2		//splash damage


projectileinfo 
{
name				"blasterbullet"
damage				9
damagetype			$evalint(IMPACT_DAMAGE)
}

weaponinfo 
{
name				"Blaster"
number				WEAPONINDEX_BLASTER
projectile			"blasterbullet"
numprojectiles			1
speed				1400
}

projectileinfo 
{
name				"machinegunbullet"
damage				5
damagetype			$evalint(IMPACT_DAMAGE)
}

weaponinfo 
{
name				"Machinegun"
number				WEAPONINDEX_MACHINEGUN
projectile			"machinegunbullet"
numprojectiles			1
reload				2000
//hspread 		200
//vspread			200
speed				0
}

projectileinfo 
{
name				"painsawdamage"
damage				15
damagetype			$evalint(IMPACT_DAMAGE)
}

weaponinfo 
{
name				"Painsaw"
number				WEAPONINDEX_PAINSAW
projectile			"painsawdamage"
numprojectiles			1
speed				0
}

projectileinfo
{
name				"shotgunbullet"
damage				7
damagetype			$evalint(IMPACT_DAMAGE)
}

weaponinfo
{
name				"Shotgun"
number				WEAPONINDEX_SHOTGUN
projectile			"shotgunbullet"
numprojectiles		8
speed				0
}

projectileinfo
{
name				"lasgunbullet"
damage				9
damagetype			$evalint(IMPACT_DAMAGE)
}

weaponinfo
{
name				"Lasgun"
number				WEAPONINDEX_LAS_GUN
projectile			"lasgunbullet"
numprojectiles		1
speed				0
}

projectileinfo
{
name				"massdriverbullet"
damage				38
damagetype			$evalint(IMPACT_DAMAGE)
}

weaponinfo
{
name				"Mass Driver"
number				WEAPONINDEX_MASS_DRIVER
projectile			"massdriverbullet"
numprojectiles		1
speed				0
}

projectileinfo 
{
name				"chaingunbullet"
damage				6
damagetype			$evalint(IMPACT_DAMAGE)
}

weaponinfo 
{
name				"Chaingun"
number				WEAPONINDEX_CHAINGUN
projectile			"chaingunbullet"
numprojectiles			1
speed				0
} 

projectileinfo
{
name				"pulseriflebullet"
damage				9
damagetype			$evalint(IMPACT_DAMAGE)
}

weaponinfo
{
name				"Pulse rifle"
number				WEAPONINDEX_MASS_DRIVER
projectile			"pulseriflebullet"
numprojectiles		1
speed				1000
}

projectileinfo
{
name				"flamethrowerbullet"
damage				20
//radius				50
damagetype			$evalint(IMPACT_DAMAGE)
}

weaponinfo
{
name				"Flame Thrower"
number				WEAPONINDEX_FLAMER
projectile			"flamethrowerbullet"
numprojectiles		1
speed				200
}

projectileinfo
{
name				"lcannonbullet"
damage				265
//radius				50
damagetype			$evalint(IMPACT_DAMAGE|SPLASH_DAMAGE)
}

weaponinfo
{
name				"Lucifer Cannon"
number				WEAPONINDEX_LUCIFER_CANNON
projectile			"lcannonbullet"
numprojectiles		1
speed				350
}