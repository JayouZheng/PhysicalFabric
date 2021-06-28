/*********************************************************************
 * FabricCS.hlsl
 * Copyright (C) 2021 Jayou. All Rights Reserved. 
 * 
 * .
 *********************************************************************/

// For updating the simulation.
cbuffer cbUpdateSettings : register(b0)
{
	float gKspring;
	float gKdamper;
	float gKwind;
	
    float gDisturbMagOrSpatialStep;
	int2 gDisturbIndex;
    int2 gGridSize;
    float3 gWind;
};
 
RWTexture2D<float4> gPrevVertex : register(u0);
RWTexture2D<float4> gCurrVertex : register(u1);
RWTexture2D<float4> gCurrVelocity : register(u2);
 
[numthreads(16, 16, 1)]
void UpdateFabricCS(int3 dispatchThreadID : SV_DispatchThreadID)
{
	// We do not need to do bounds checking because:
	//	 *out-of-bounds reads return 0, which works for us--it just means the boundary of 
	//    our water simulation is clamped to 0 in local space.
	//   *out-of-bounds writes are a no-op.
	
	int x = dispatchThreadID.x;
	int y = dispatchThreadID.y;

    float dt = 0.02f;
    float3 g = { 0.0f, 0.0f, -9.8f };
    float m = 0.2f;

    float3 Fspring = { 0.0f, 0.0f, 0.0f };
    float3 Fdamper = { 0.0f, 0.0f, 0.0f };
    float3 Fall = { 0.0f, 0.0f, 0.0f };
    if (x - 1 >= 0)
    {
        float3 left1 = gCurrVertex[int2(x - 1, y)].xyz - gCurrVertex[int2(x, y)].xyz;
        Fspring += gKspring * (length(left1) - gDisturbMagOrSpatialStep) * normalize(left1);
        Fdamper += gKdamper * (gCurrVelocity[int2(x - 1, y)].xyz - gCurrVelocity[int2(x, y)].xyz);
        // Fall += Fspring + Fdamper;
    }
    
    if (x - 2 >= 0)
    {
        float3 left2 = gCurrVertex[int2(x - 2, y)].xyz - gCurrVertex[int2(x, y)].xyz;
        Fspring += gKspring * (length(left2) - 2 * gDisturbMagOrSpatialStep) * normalize(left2);
        Fdamper += gKdamper * (gCurrVelocity[int2(x - 2, y)].xyz - gCurrVelocity[int2(x, y)].xyz);
        // Fall += Fspring + Fdamper;
    }

    if (x + 1 <= gGridSize.x - 1)
    {
        float3 right1 = gCurrVertex[int2(x + 1, y)].xyz - gCurrVertex[int2(x, y)].xyz;
        Fspring += gKspring * (length(right1) - gDisturbMagOrSpatialStep) * normalize(right1);
        Fdamper += gKdamper * (gCurrVelocity[int2(x + 1, y)].xyz - gCurrVelocity[int2(x, y)].xyz);
        // Fall += Fspring + Fdamper;
    }

    if (x + 2 <= gGridSize.x - 1)
    {
        float3 right2 = gCurrVertex[int2(x + 2, y)].xyz - gCurrVertex[int2(x, y)].xyz;
        Fspring += gKspring * (length(right2) - 2 * gDisturbMagOrSpatialStep) * normalize(right2);
        Fdamper += gKdamper * (gCurrVelocity[int2(x + 2, y)].xyz - gCurrVelocity[int2(x, y)].xyz);
        // Fall += Fspring + Fdamper;
    }

    if (y - 1 >= 0)
    {
        float3 up1 = gCurrVertex[int2(x, y - 1)].xyz - gCurrVertex[int2(x, y)].xyz;
        Fspring += gKspring * (length(up1) - gDisturbMagOrSpatialStep) * normalize(up1);
        Fdamper += gKdamper * (gCurrVelocity[int2(x, y - 1)].xyz - gCurrVelocity[int2(x, y)].xyz);
        // Fall += Fspring + Fdamper;
    }

    if (y - 2 >= 0)
    {
        float3 up2 = gCurrVertex[int2(x, y - 2)].xyz - gCurrVertex[int2(x, y)].xyz;
        Fspring += gKspring * (length(up2) - 2 * gDisturbMagOrSpatialStep) * normalize(up2);
        Fdamper += gKdamper * (gCurrVelocity[int2(x, y - 2)].xyz - gCurrVelocity[int2(x, y)].xyz);
        // Fall += Fspring + Fdamper;
    }

    if (y + 1 <= gGridSize.y - 1)
    {
        float3 down1 = gCurrVertex[int2(x, y + 1)].xyz - gCurrVertex[int2(x, y)].xyz;
        Fspring += gKspring * (length(down1) - gDisturbMagOrSpatialStep) * normalize(down1);
        Fdamper += gKdamper * (gCurrVelocity[int2(x, y + 1)].xyz - gCurrVelocity[int2(x, y)].xyz);
        // Fall += Fspring + Fdamper;
    }

    if (y + 2 <= gGridSize.y - 1)
    {
        float3 down2 = gCurrVertex[int2(x, y + 2)].xyz - gCurrVertex[int2(x, y)].xyz;
        Fspring += gKspring * (length(down2) - 2 * gDisturbMagOrSpatialStep) * normalize(down2);
        Fdamper += gKdamper * (gCurrVelocity[int2(x, y + 2)].xyz - gCurrVelocity[int2(x, y)].xyz);
        // Fall += Fspring + Fdamper;
    }

    if (x - 1 >= 0 && y - 1 >= 0)
    {
        float3 leftup = gCurrVertex[int2(x - 1, y - 1)].xyz - gCurrVertex[int2(x, y)].xyz;
        Fspring += gKspring * (length(leftup) - sqrt(2) * gDisturbMagOrSpatialStep) * normalize(leftup);
        Fdamper += gKdamper * (gCurrVelocity[int2(x - 1, y - 1)].xyz - gCurrVelocity[int2(x, y)].xyz);
        // Fall += Fspring + Fdamper;
    }

    if (x - 1 >= 0 && y + 1 <= gGridSize.y - 1)
    {
        float3 leftdown = gCurrVertex[int2(x - 1, y + 1)].xyz - gCurrVertex[int2(x, y)].xyz;
        Fspring += gKspring * (length(leftdown) - sqrt(2) * gDisturbMagOrSpatialStep) * normalize(leftdown);
        Fdamper += gKdamper * (gCurrVelocity[int2(x - 1, y + 1)].xyz - gCurrVelocity[int2(x, y)].xyz);
        // Fall += Fspring + Fdamper;
    }

    if (x + 1 <= gGridSize.x - 1 && y - 1 >= 0)
    {
        float3 rightup = gCurrVertex[int2(x + 1, y - 1)].xyz - gCurrVertex[int2(x, y)].xyz;
        Fspring += gKspring * (length(rightup) - sqrt(2) * gDisturbMagOrSpatialStep) * normalize(rightup);
        Fdamper += gKdamper * (gCurrVelocity[int2(x + 1, y - 1)].xyz - gCurrVelocity[int2(x, y)].xyz);
        // Fall += Fspring + Fdamper;
    }

    if (x + 1 <= gGridSize.x - 1 && y + 1 <= gGridSize.y - 1)
    {
        float3 rightdown = gCurrVertex[int2(x + 1, y + 1)].xyz - gCurrVertex[int2(x, y)].xyz;
        Fspring += gKspring * (length(rightdown) - sqrt(2) * gDisturbMagOrSpatialStep) * normalize(rightdown);
        Fdamper += gKdamper * (gCurrVelocity[int2(x + 1, y + 1)].xyz - gCurrVelocity[int2(x, y)].xyz);
        // Fall += Fspring + Fdamper;
    }

    float3 Fextern = m * g;
    /*
    if (x == 0 && y == 0)
    {
        Fextern -= 65535 * m * g;
    }
    */
    Fall = Fspring + Fdamper + Fextern + gWind;
    if (length(gCurrVelocity[int2(x, y)].xyz) > 0)
    {
        Fall -= 0.4f * normalize(gCurrVelocity[int2(x, y)].xyz);
    }
    gPrevVertex[int2(x, y)] = float4(gCurrVertex[int2(x, y)].xyz + gCurrVelocity[int2(x, y)].xyz * dt + (Fall / (2 * m)) * dt * dt, 0.0f);

    gCurrVelocity[int2(x, y)] = float4(gCurrVelocity[int2(x, y)].xyz + (Fall / m) * dt, 0.0f);

    if (y == 0)
    {
        gPrevVertex[int2(x, y)] = gCurrVertex[int2(x, y)];
        gCurrVelocity[int2(x, y)] = float4(0.0f,0.0f,0.0f,0.0f);
    }
}

[numthreads(1, 1, 1)]
void DisturbFabricCS(int3 groupThreadID : SV_GroupThreadID,
                    int3 dispatchThreadID : SV_DispatchThreadID)
{
	// We do not need to do bounds checking because:
	//	 *out-of-bounds reads return 0, which works for us--it just means the boundary of 
	//    our water simulation is clamped to 0 in local space.
	//   *out-of-bounds writes are a no-op.
	
	int x = gDisturbIndex.x;
	int y = gDisturbIndex.y;
    
}
