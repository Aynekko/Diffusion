//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#ifndef R_VIEW_H
#define R_VIEW_H

// Definition for how to calculate a point on the remap curve
enum RemapAngleRange_CurvePart_t
{
	RemapAngleRange_CurvePart_Zero = 0,
	RemapAngleRange_CurvePart_Spline,
	RemapAngleRange_CurvePart_Linear,
};

// If we enter the linear part of the remap for curve for any degree of freedom, we can lock
// that DOF (stop remapping). This is useful for making flips feel less spastic as we oscillate
// randomly between different parts of the remapping curve.
struct ViewLockData_t
{
	float	flLockInterval;		// The duration to lock the view when we lock it for this degree of freedom.
					// 0 = never lock this degree of freedom.
	bool	bLocked;			// True if this DOF was locked because of the above condition.
	float	flUnlockTime;		// If this DOF is locked, the time when we will unlock it.
	float	flUnlockBlendInterval;	// If this DOF is locked, how long to spend blending out of the locked view when we unlock.
};

void V_Init( void );
void V_VidInit( void );
void V_StartPitchDrift( void );
void V_StopPitchDrift( void );

void V_CalcFirstPersonRefdef( struct ref_params_s *pparams );

void V_ChangeView(void);

extern void V_GetInEyePos(int target, Vector& origin, Vector& angles);
extern void V_GetChasePos(int target, Vector& cl_angles, Vector& origin, Vector& angles);
extern void V_GetChaseOrigin(const Vector& angles, const Vector& origin, float distance, Vector& returnvec);
int V_FindViewModelByWeaponModel(int weaponindex);

extern float MouseSpeed;

#endif//R_VIEW_H