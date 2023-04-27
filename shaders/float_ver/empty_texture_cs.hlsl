RWTexture3D<float> tex : register(u0);

#define MAX_DIST 10000.

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID) {
    // tex[id] = MAX_DIST;
    tex[id] = 16.;
}
