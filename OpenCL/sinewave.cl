__kernel void sinewave_kernel(__global float4* pos , unsigned int width ,unsigned int hight,float time) {
	unsigned int x = get_global_id(0);
	unsigned int y = get_global_id(1);
	float u = x / (float)width;
	float v = y / (float)hight;
	u = u * 2.0f - 1.0f;
	v = v * 2.0f - 1.0f;
	float freq = 4.0f;
	float w = sin(u * freq + time) * cos(v * freq + time) * 0.5f;
	pos[y * width + x] = (float4)(u, w, v, 1.0f);
}