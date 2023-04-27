RWTexture3D<int> tex : register(u0);

#define MAX_DIST 127

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID) {
    tex[id] = MAX_DIST;
}
