
struct FNoiseParameter
{
	float	Base;
	float NoiseScale;
	float NoiseAmount;

	// Constructors.
	FNoiseParameter() {}
	FNoiseParameter(float InBase, float InScale, float InAmount) :
		Base(InBase),
		NoiseScale(InScale),
		NoiseAmount(InAmount)
	{}

	// Sample
	float Sample(int32 X, int32 Y) const
	{
		float	Noise = 0.0f;
		X = FMath::Abs(X);
		Y = FMath::Abs(Y);

		if (NoiseScale > DELTA)
		{
			for (uint32 Octave = 0; Octave < 4; Octave++)
			{
				float	OctaveShift = 1 << Octave;
				float	OctaveScale = OctaveShift / NoiseScale;
				Noise += PerlinNoise2D(X * OctaveScale, Y * OctaveScale) / OctaveShift;
			}
		}

		return Base + Noise * NoiseAmount;
	}

private:
	static const int32 Permutations[256];

	bool operator==(const FNoiseParameter& SrcNoise)
	{
		if ((Base == SrcNoise.Base) &&
			(NoiseScale == SrcNoise.NoiseScale) &&
			(NoiseAmount == SrcNoise.NoiseAmount))
		{
			return true;
		}

		return false;
	}

	void operator=(const FNoiseParameter& SrcNoise)
	{
		Base = SrcNoise.Base;
		NoiseScale = SrcNoise.NoiseScale;
		NoiseAmount = SrcNoise.NoiseAmount;
	}


	float Fade(float T) const
	{
		return T * T * T * (T * (T * 6 - 15) + 10);
	}


	float Grad(int32 Hash, float X, float Y) const
	{
		int32		H = Hash & 15;
		float	U = H < 8 || H == 12 || H == 13 ? X : Y,
			V = H < 4 || H == 12 || H == 13 ? Y : 0;
		return ((H & 1) == 0 ? U : -U) + ((H & 2) == 0 ? V : -V);
	}

	float PerlinNoise2D(float X, float Y) const
	{
		int32		TruncX = FMath::Trunc(X),
			TruncY = FMath::Trunc(Y),
			IntX = TruncX & 255,
			IntY = TruncY & 255;
		float	FracX = X - TruncX,
			FracY = Y - TruncY;

		float	U = Fade(FracX),
			V = Fade(FracY);

		int32	A = Permutations[IntX] + IntY,
			AA = Permutations[A & 255],
			AB = Permutations[(A + 1) & 255],
			B = Permutations[(IntX + 1) & 255] + IntY,
			BA = Permutations[B & 255],
			BB = Permutations[(B + 1) & 255];

		return	FMath::Lerp(FMath::Lerp(Grad(Permutations[AA], FracX, FracY),
			Grad(Permutations[BA], FracX - 1, FracY), U),
			FMath::Lerp(Grad(Permutations[AB], FracX, FracY - 1),
			Grad(Permutations[BB], FracX - 1, FracY - 1), U), V);
	}
};

const int32 FNoiseParameter::Permutations[256] =
{
	151, 160, 137, 91, 90, 15,
	131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23,
	190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
	88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166,
	77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244,
	102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196,
	135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123,
	5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42,
	223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
	129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228,
	251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107,
	49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
	138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180
};
