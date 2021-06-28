// 2019-9-28

#pragma once

#include <windows.h>
#include <string>
#include <shobjidl.h> // The Common Item Dialog implements an interface named IFileOpenDialog, which is declared in the header file Shobjidl.h.
#include <DirectXMath.h>

using namespace DirectX;

class DXHelper
{
public:
	//
	static bool OpenDialogBox(HWND owner, std::wstring& get_file_path)
	{
		get_file_path = L"";
		HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
			COINIT_DISABLE_OLE1DDE);
		if (SUCCEEDED(hr))
		{
			IFileOpenDialog *pFileOpen;

			// Create the FileOpenDialog object.
			hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
				IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

			if (SUCCEEDED(hr))
			{
				// Show the Open dialog box.
				hr = pFileOpen->Show(owner);

				// Get the file name from the dialog box.
				if (SUCCEEDED(hr))
				{
					IShellItem *pItem;
					hr = pFileOpen->GetResult(&pItem);
					if (SUCCEEDED(hr))
					{
						PWSTR pszFilePath;
						hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

						// Display the file name to the user.
						if (SUCCEEDED(hr))
						{
							get_file_path = std::wstring(pszFilePath);
							// MessageBox(NULL, pszFilePath, L"File Path", MB_OK);
							CoTaskMemFree(pszFilePath);
						}
						pItem->Release();
					}
				}
				pFileOpen->Release();
			}
			CoUninitialize();
		}
		return true;
	}

	//
	static std::wstring StringToWString(const std::string& str)
	{
		int num = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
		wchar_t *wide = new wchar_t[num];
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wide, num);
		std::wstring w_str(wide);
		delete[] wide;
		return w_str;
	}

	// Data converted from UTF-16 to non-Unicode encodings is subject to data loss,
	// because a code page might not be able to represent every character used in the specific Unicode data.
	static std::string WStringToString(const std::wstring& w_str)
	{
		int num = WideCharToMultiByte(CP_UTF8, 0, w_str.c_str(), -1, NULL, 0, NULL, NULL);
		char* wide = new char[num];
		WideCharToMultiByte(CP_UTF8, 0, w_str.c_str(), -1, wide, num, NULL, NULL);
		std::string str(wide);
		delete[] wide;
		return str;
	}

	struct LineVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT4 Color;
	};

	using uint16 = std::uint16_t;
	using uint32 = std::uint32_t;

	struct MeshData
	{
		std::vector<LineVertex> Vertices;
		std::vector<uint32> Indices32;

		std::vector<uint16>& GetIndices16()
		{
			if (mIndices16.empty())
			{
				mIndices16.resize(Indices32.size());
				for (size_t i = 0; i < Indices32.size(); ++i)
					mIndices16[i] = static_cast<uint16>(Indices32[i]);
			}

			return mIndices16;
		}

	private:
		std::vector<uint16> mIndices16;
	};

	///<summary>
	/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
	/// at the origin with the specified width and depth.
	///</summary>
	static MeshData CreateLineGrid(float width, float depth, uint32 m, uint32 n)
	{
		MeshData meshData;

		uint32 vertexCount = (m + 1) * (n + 1);
		uint32 lineCount = m + n + 2;

		//
		// Create the vertices.
		// 多余的顶点保留后用

		float halfWidth = 0.5f*width;
		float halfDepth = 0.5f*depth;

		float dx = width / n;
		float dz = depth / m;

		meshData.Vertices.resize(vertexCount);
		for (uint32 i = 0; i < m + 1; ++i)
		{
			float z = halfDepth - i * dz;
			for (uint32 j = 0; j < n + 1; ++j)
			{
				float x = -halfWidth + j * dx;

				meshData.Vertices[i*(n + 1) + j].Pos = XMFLOAT3(x, 0.0f, z);

				meshData.Vertices[i*(n + 1) + j].Color = XMFLOAT4(Colors::Gray);

				if (i % 10 == 0 && j % 10 == 0)
				{
					meshData.Vertices[i*(n + 1) + j].Color = XMFLOAT4(Colors::Blue);
				}

				if (i == m / 2)
				{
					meshData.Vertices[i*(n + 1) + j].Color = XMFLOAT4(Colors::Green);
				}
				else if (j == n / 2)
				{
					meshData.Vertices[i*(n + 1) + j].Color = XMFLOAT4(Colors::Red);
				}
			}
		}

		//
		// Create the indices.
		// D3D_PRIMITIVE_TOPOLOGY_LINELIST.

		meshData.Indices32.resize(lineCount * 2); // 2 indices per line

		// Iterate over each quad and compute indices.
		uint32 k = 0;
		for (uint32 i = 0; i < n + 1; ++i)
		{
			meshData.Indices32[k] = i;
			meshData.Indices32[k + 1] = (n + 1) * m + i;

			k += 2; // next line
		}
		for (uint32 i = 0; i < m + 1; ++i)
		{
			meshData.Indices32[k] = i * (n + 1);
			meshData.Indices32[k + 1] = i * (n + 1) + n;

			k += 2; // next line
		}

		return meshData;
	}
};