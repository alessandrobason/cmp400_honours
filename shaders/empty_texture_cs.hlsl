RWTexture3D<float> tex : register(u0);

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID) {
    tex[id] = 9.0;
}
