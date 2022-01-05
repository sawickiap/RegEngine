module;

#include "BaseUtils.hpp"

export module BaseUtils;

export std::vector<char> LoadFile(const wstr_view& path)
{
	FILE* f;
	CHECK_BOOL(_wfopen_s(&f, path.c_str(), L"rb") == 0);
	_fseeki64(f, 0, SEEK_END);
	const size_t size = (size_t)_ftelli64(f);
	_fseeki64(f, 0, SEEK_SET);
	std::vector<char> data(size);
	if(size)
	{
		fread(data.data(), 1, size, f);
	}
	fclose(f);
	return data;
}
