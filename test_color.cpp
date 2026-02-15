#include <iostream>
#include <cmath>
#include <algorithm>

typedef unsigned char byte;

float clamp(float x, float min, float max)
{
	if (x < min) return min;
	if (x > max) return max;
	return x;
}

void oklch_to_rgb(float l, float c, float h, float& r, float& g, float& b)
{
	float hr = h * 3.14159265358979323846f / 180.0f;
	float a_ = c * cos(hr);
	float b_ = c * sin(hr);

	float l_2 = l + 0.3963377774f * a_ + 0.2158037573f * b_;
	float m_2 = l - 0.1055613458f * a_ - 0.0638541728f * b_;
	float s_2 = l - 0.0894841775f * a_ - 1.2914855480f * b_;

	float l3 = l_2 * l_2 * l_2;
	float m3 = m_2 * m_2 * m_2;
	float s3 = s_2 * s_2 * s_2;

	float r_lin = +4.0767416621f * l3 - 3.3077115913f * m3 + 0.2309699292f * s3;
	float g_lin = -1.2684380046f * l3 + 2.6097574011f * m3 - 0.3413193965f * s3;
	float b_lin = -0.0041960863f * l3 - 0.7034186147f * m3 + 1.7076127010f * s3;

	auto gamma = [](float x)
	{
		return x <= 0.0031308f ? 12.92f * x : 1.055f * pow(x, 1.0f / 2.4f) - 0.055f;
	};

	r = gamma(r_lin);
	g = gamma(g_lin);
	b = gamma(b_lin);
}

int main() {
    float r, g, b;
    oklch_to_rgb(0.45f, 0.24f, 277.023f, r, g, b);
    r = clamp(r, 0, 1);
    g = clamp(g, 0, 1);
    b = clamp(b, 0, 1);
    std::cout << "R: " << (int)round(r * 255) << std::endl;
    std::cout << "G: " << (int)round(g * 255) << std::endl;
    std::cout << "B: " << (int)round(b * 255) << std::endl;
    return 0;
}
