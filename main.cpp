#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <Windows.h>

struct mat3x3
{
	float at[9];

	void clear(float value)
	{
		for (int i = 0; i < 9; i++)
			at[i] = value;
	}
};
struct vec3
{
	float x, y, z;

	float length_squared()
	{
		return x * x + y * y + z * z;
	}
	float length()
	{
		return sqrtf(length_squared());
	}
	vec3 normalized()
	{
		return *this / length();
	}

	vec3 operator+ (const vec3& other) const
	{
		return { x + other.x, y + other.y, z + other.z };
	}
	vec3 operator- (const vec3& other) const
	{
		return { x - other.x, y - other.y, z - other.z };
	}
	vec3 operator- () const
	{
		return { -x, -y, -z };
	}
	float operator* (const vec3& other) const
	{
		return x * other.x + y * other.y + z * other.z;
	}
	vec3 operator/ (float divisor) const
	{
		return { x / divisor, y / divisor, z / divisor };
	}
	vec3 operator* (float multiplier) const
	{
		return {x * multiplier, y * multiplier, z * multiplier};
	}

	void operator+=(const vec3& other)
	{
		*this = *this + other;
	}
	void operator-=(const vec3& other)
	{
		*this = *this - other;
	}

	vec3 operator* (const mat3x3& matrix) const
	{
		return {	matrix.at[0] * x + matrix.at[1] * y + matrix.at[2] * z,
					matrix.at[3] * x + matrix.at[4] * y + matrix.at[5] * z,
					matrix.at[6] * x + matrix.at[7] * y + matrix.at[8] * z };
	}
};
struct rgb
{
	unsigned char r, b, g;
};

struct Ray
{
	vec3 origin;
	vec3 direction;
};

struct Object
{
	rgb color;
	Object* next;

	Object(rgb color) 
		: color(color), next(0) {}

	virtual bool ray_intersect(const Ray& ray, float* _distance = 0, vec3* _hit = 0, vec3* _normal = 0) = 0;
};

struct Sphere : Object
{
	vec3 position;
	float radius;

	Sphere(vec3 position, float radius, rgb color)
		: Object(color), position(position), radius(radius) {}

	virtual bool ray_intersect(const Ray& ray, float* _distance = 0, vec3* _hit = 0, vec3* _normal = 0) override
	{
		vec3 s = position - ray.origin;

		float t = s * ray.direction;

		if (t < 0.0f)
			return false;

		vec3 projected = ray.origin + ray.direction * t;
		vec3 d = projected - position;

		float t1 = sqrtf(radius * radius - (d.x * d.x + d.y * d.y + d.z * d.z));

		if (d.x * d.x + d.y * d.y + d.z * d.z <= radius * radius)
		{
			if (_distance)
				*_distance = t - t1;
			if (_hit)
				*_hit = ray.origin + ray.direction * (t - t1);
			if (_normal)
			{
				if(_hit)
					*_normal = (*_hit - position) / radius;
				else
					*_normal = (ray.origin + ray.direction * (t - t1) - position) / radius;
			}

			return true;
		}

		return false;
	}
};
struct Plane : Object
{
	vec3 position;
	vec3 normal;

	Plane(vec3 position, vec3 normal, rgb color) 
		: Object(color), position(position), normal(normal) {}

	virtual bool ray_intersect(const Ray& ray, float* _distance = 0, vec3* _hit = 0, vec3* _normal = 0) override
	{
		vec3 s = position - ray.origin;

		float t = (s * normal) / (ray.direction * normal);

		if (t > 0.00001f)
		{
			if (_distance)
				*_distance = t;
			if (_hit)
				*_hit = ray.origin + ray.direction * t;
			if (_normal)
				*_normal = normal;

			return true;
		}

		return false;
	}
};
struct Triangle : Object
{
	vec3 vertecies[3];

	Triangle(vec3 v1, vec3 v2, vec3 v3, rgb color) 
		: Object(color) 
	{
		vertecies[0] = v1;
		vertecies[1] = v2;
		vertecies[2] = v3;
	}

	virtual bool ray_intersect(const Ray& ray, float* _distance = 0, vec3* _hit = 0, vec3* _normal = 0) override
	{
		vec3 a = vertecies[1] - vertecies[0];
		vec3 b = vertecies[2] - vertecies[0];

		float distance;
		vec3 hit;
		vec3 normal;
		
		Plane plane = Plane(vertecies[0], { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }, { 0 ,0 ,0 });
		if (!plane.ray_intersect(ray, &distance, &hit, &normal))
			return false;

		if (_distance)
			*_distance = distance;
		if (_hit)
			*_hit = hit;
		if (_normal)
			*_normal = normal;

		for (int i = 0; i < 3; i++)
		{
			vec3 s = hit - vertecies[i];

			vec3 u;
			if (i == 2)
				u = vertecies[0] - vertecies[i];
			else
				u = vertecies[i + 1] - vertecies[i];

			float t = s * u;

			if (t > u.length_squared() || t < 0.0f)
				return false;
		}

		return true;
	}
};

void specular_reflect(Ray* ray, const vec3& hit, const vec3& normal)
{
	ray->origin = hit - ray->direction;
	vec3 u = hit - ray->origin;

	float t = u * normal;

	ray->direction = (hit + normal * t) * 2.0f - ray->origin;
	ray->origin = hit;
}

int width = 32;
int height = 32;

const float fov = 2.0f;

vec3 camera = { 0.0f, 1.0f, 0.0f };

vec3 forwardNormal = { 0.0f, 0.0f, 1.0f };
vec3 upNormal = { 0.0f, -1.0f, 0.0f };
vec3 rightNormal = { 1.0f, 0.0f, 0.0f };

Object* start = 0;
Object* last = 0;

Plane ground = { {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {255, 255, 255} };

char* buffer;
char* tempBuffer;

void add(Object* obj)
{
	if (start == 0)
	{
		start = obj;
		last = start;

		return;
	}

	last->next = obj;
	last = obj;
}

void generate_buffer()
{
	delete[] buffer;
	delete[] tempBuffer;

	buffer = new char[21 * width * height + 1];
	tempBuffer = new char[21 * width + 2];

	for (int i = 0; i < width * height; i++)
		memcpy(buffer + i * 21, "\x1b[38;2;000;000;000m  ", 21);

	buffer[21 * width * height] = 0;
}

inline char decimal_to_char(unsigned char decimal, int offset)
{
	return (decimal / offset) % 10 + 48;
}
void set_pixel(int index, rgb color, char texture)
{
	buffer[index * 21 + 7] = decimal_to_char(color.r, 100);
	buffer[index * 21 + 8] = decimal_to_char(color.r, 10);
	buffer[index * 21 + 9] = decimal_to_char(color.r, 1);

	buffer[index * 21 + 11] = decimal_to_char(color.g, 100);
	buffer[index * 21 + 12] = decimal_to_char(color.g, 10);
	buffer[index * 21 + 13] = decimal_to_char(color.g, 1);

	buffer[index * 21 + 15] = decimal_to_char(color.b, 100);
	buffer[index * 21 + 16] = decimal_to_char(color.b, 10);
	buffer[index * 21 + 17] = decimal_to_char(color.b, 1);

	buffer[index * 21 + 19] = texture;
	buffer[index * 21 + 20] = texture;
}

int main()
{
	SetConsoleTitle(L"Console Raytracing");

	(void)getchar();
	printf("\x1b[?25l");

	generate_buffer();

	srand(clock());

	for (int i = 0; i < 10; i++)
	{
		add(new Sphere
		(
			{
				(rand() % 20) - 10.0f,
				(rand() % 10) + 1.0f,
				(rand() % 6) + 10.0f
			},

			(rand() % 2) + 1.0f,

			{
				(unsigned char)((rand() % 200) + 55),
				(unsigned char)((rand() % 200) + 55),
				(unsigned char)((rand() % 200) + 55)
			}
		));
	}

	add(new Plane({ 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 255, 255, 255 }));
	//add(new Triangle({ 0.0f, 2.0f, 5.0f }, { 2.0f, 1.0f, 5.0f }, { 1.0f, 4.0f, 5.0f }, { 255, 0, 255 }));

	time_t last = 0;

	float zoomTimer = 0.0f;

	float speed = 2.0f;

	float angleX = 0.0f;
	float angleY = 0.0f;

	mat3x3 pitch;
	mat3x3 roll;

	vec3 forward;
	vec3 up;
	vec3 right;

	POINT lastCursor;
	SetCursorPos(1920 / 2, 1080 / 2);

	while (1)
	{
		float elapsedTime = (float)(clock() - last) / CLOCKS_PER_SEC;
		float time = (float)clock() / CLOCKS_PER_SEC;

		last = clock();

		GetCursorPos(&lastCursor);
		SetCursorPos(1920 / 2, 1080 / 2);

		lastCursor.x -= 1920 / 2;
		lastCursor.y -= 1080 / 2;

		angleX += lastCursor.y * 0.0025f;
		angleY += lastCursor.x * 0.0025f;

		pitch.clear(0.0f);
		pitch.at[0] = cosf(angleY);
		pitch.at[2] = sinf(angleY);
		pitch.at[4] = 1.0f;
		pitch.at[6] = -sinf(angleY);
		pitch.at[8] = cosf(angleY);

		roll.clear(0.0f);
		roll.at[0] = 1.0f;
		roll.at[4] = cosf(angleX);
		roll.at[5] = -sinf(angleX);
		roll.at[7] = sinf(angleX);
		roll.at[8] = cosf(angleX);

		forward = forwardNormal * roll * pitch;
		up = upNormal * roll * pitch;
		right = rightNormal * roll * pitch;

		if (GetAsyncKeyState('V'))
			speed = 10.0f;
		else
			speed = 2.0f;
		if (GetAsyncKeyState('W'))
			camera += forwardNormal * pitch * elapsedTime * speed;
		if (GetAsyncKeyState('S'))
			camera -= forwardNormal * pitch * elapsedTime * speed;
		if (GetAsyncKeyState('A'))
			camera -= rightNormal * pitch * elapsedTime * speed;
		if (GetAsyncKeyState('D'))
			camera += rightNormal * pitch * elapsedTime * speed;
		if (GetAsyncKeyState(VK_SPACE))
			camera -= upNormal * pitch * elapsedTime * speed;
		if (GetAsyncKeyState(VK_LSHIFT))
			camera += upNormal * pitch * elapsedTime * speed;
		if (GetAsyncKeyState('Q') && width < 78 && zoomTimer <= time)
		{
			width += 2;
			height += 2;

			generate_buffer();

			printf("\x1b[2J");

			zoomTimer = time + 0.1f;
		}
		if (GetAsyncKeyState('E') && width > 4 && zoomTimer <= time)
		{
			width -= 2;
			height -= 2;

			generate_buffer();

			printf("\x1b[2J");

			zoomTimer = time + 0.1f;
		}
		if (GetAsyncKeyState(VK_ESCAPE))
			return 0;

		for (int y = -height / 2; y < height / 2; y++)
		{
			for (int x = -width / 2; x < width / 2; x++)
			{
				Ray ray;
				ray.origin = camera;
				ray.direction = right * ((float)x / (float)width) * fov + up * ((float)y / (float)height) * fov + forward;
				ray.direction = ray.direction.normalized();

				vec3 hit = { 0.0f, 0.0f };
				vec3 normal = { 0.0f, 0.0f };
				float distance = 0.0f;

				float smallestDist = -1.0f;
				rgb smallestColor;
				vec3 smallestHit;
				vec3 smallestNormal;

				bool shaded = false;

				for (Object* obj = start; obj; obj = obj->next)
				{
					if (obj->ray_intersect(ray, &distance, &hit, &normal))
					{
						if (distance < smallestDist || smallestDist < 0.0f)
						{
							smallestDist = distance;
							smallestColor = obj->color;
							smallestHit = hit;
							smallestNormal = normal;
						}
					}
				}

				if (smallestDist >= 0.0f)
				{
					Ray sunray;
					sunray.origin = smallestHit;
					sunray.direction = { 0.0f, 1.0f, 0.0f };

					for (Object* obj = start; obj; obj = obj->next)
					{
						if (obj->ray_intersect(sunray))
						{
							shaded = true;
							break;
						}
					}
				}

				if (smallestDist < 0.0f)
					set_pixel((x + width / 2) + (y + height / 2) * width, {0,0,0}, ' ');
				else if(shaded)
					set_pixel((x + width / 2) + (y + height / 2) * width, smallestColor, '.');
				else
					set_pixel((x + width / 2) + (y + height / 2) * width, smallestColor, '#');
			}
		}

		printf("\x1b[H");
		
		for (int i = 0; i < height; i++)
		{
			memcpy(tempBuffer, buffer + i * width * 21, width * 21);

			tempBuffer[width * 21] = '\n';
			tempBuffer[width * 21 + 1] = 0;

			printf(tempBuffer);
		}

		printf("\x1b[38;2;255;255;255mScreen: %d, %d\nCamera: %3.2f, %3.2f, %3.2f\nOrientation: %3.2f, %3.2f\nFPS: %3.2f\nTime: %3.2f", width, height, camera.x, camera.y, camera.z, angleX, angleY, 1.0f / elapsedTime, time);
	}
}