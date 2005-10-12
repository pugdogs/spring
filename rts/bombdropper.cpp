// Cannon.cpp: implementation of the CBombDropper class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "bombdropper.h"
#include "Unit.h"
#include "ExplosiveProjectile.h"
#include "InfoConsole.h"
#include "Sound.h"
#include "GameHelper.h"
#include "Team.h"
#include "WeaponProjectile.h"
#include "WeaponDefHandler.h"
#include "TorpedoProjectile.h"
//#include "mmgr.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CBombDropper::CBombDropper(CUnit* owner,bool useTorps)
: CWeapon(owner),
	dropTorpedoes(useTorps),
	bombMoveRange(3),
	tracking(0)
{
	onlyForward=true;
}

CBombDropper::~CBombDropper()
{

}

void CBombDropper::Update()
{
	if(targetType!=Target_None){
		weaponPos=owner->pos+owner->frontdir*relWeaponPos.z+owner->updir*relWeaponPos.y+owner->rightdir*relWeaponPos.x;
		subClassReady=false;
		if(targetType==Target_Unit)
			targetPos=targetUnit->pos;		//aim at base of unit instead of middle and ignore uncertainity
		if(weaponPos.y>targetPos.y){
			float d=targetPos.y-weaponPos.y;
			float s=-owner->speed.y;
			float sq=(s-2*d)/-gs->gravity;
			if(sq>0)
				predict=-s/-gs->gravity+sqrt(sq);
			else
				predict=0;
			float3 hitpos=owner->pos+owner->speed*predict;
			float speedf=owner->speed.Length();
			if(hitpos.distance2D(targetPos)<(salvoSize-1)*speedf*salvoDelay*0.5+bombMoveRange){
				subClassReady=true;
			}
		}
	}
	CWeapon::Update();
}

bool CBombDropper::TryTarget(const float3& pos,bool userTarget,CUnit* unit)
{
	if(unit){
		if(unit->isUnderWater)
			return false;
	} else {
		if(pos.y<0)
			return false;
	}
	return true;
}

void CBombDropper::Fire(void)
{
	if(targetType==Target_Unit)
		targetPos=targetUnit->pos;		//aim at base of unit instead of middle and ignore uncertainity
	if(dropTorpedoes){
		new CTorpedoProjectile(weaponPos,owner->speed,owner,damages,areaOfEffect,projectileSpeed,tracking,range/projectileSpeed+15+predict,targetUnit, weaponDef);
	} else {
		float3 dif=targetPos-owner->pos;		//fudge a bit better lateral aim to compensate for imprecise aircraft steering
		dif.y=0;
		float3 dir=owner->speed;
		dir.y=0;
		dir.Normalize();
		dif-=dir*dif.dot(dir);
		dif/=predict;
		float size=dif.Length();
		if(size>0.5)
			dif/=size*0.5;
		new CExplosiveProjectile(owner->pos,owner->speed+dif,owner,damages, weaponDef, 1000,areaOfEffect);
	}
	//CWeaponProjectile::CreateWeaponProjectile(owner->pos,owner->speed,owner, NULL, float3(0,0,0), damages, weaponDef);
	if(fireSoundId && (!weaponDef->soundTrigger || salvoLeft==salvoSize-1))
		sound->PlaySound(fireSoundId,owner,fireSoundVolume);
}

void CBombDropper::Init(void)
{
	CWeapon::Init();
	maxAngleDif=-1;
}

void CBombDropper::SlowUpdate(void)
{
	weaponDef->noAutoTarget=true;		//very bad way of making sure it wont try to find targets not targeted by the cai (to save cpu mostly)
	CWeapon::SlowUpdate();
	weaponDef->noAutoTarget=false;
}
