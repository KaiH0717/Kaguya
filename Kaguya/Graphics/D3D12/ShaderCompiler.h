#pragma once
#include "dxcapi.h"
#include "d3d12shader.h"

class Shader
{
public:
	enum class EType
	{
		Unknown,
		Vertex,
		Hull,
		Domain,
		Geometry,
		Pixel,
		Compute,
		Amplification,
		Mesh,
	};

	Shader() noexcept = default;
	Shader(EType Type, IDxcBlob* Blob, IDxcBlob* PDBBlob, std::wstring&& PDBName) noexcept
		: Type(Type)
		, Blob(Blob)
		, PDBBlob(PDBBlob)
		, PDBName(std::move(PDBName))
	{
	}

	operator D3D12_SHADER_BYTECODE() const
	{
		return D3D12_SHADER_BYTECODE{ .pShaderBytecode = Blob->GetBufferPointer(),
									  .BytecodeLength  = Blob->GetBufferSize() };
	}

private:
	EType							 Type;
	Microsoft::WRL::ComPtr<IDxcBlob> Blob;
	Microsoft::WRL::ComPtr<IDxcBlob> PDBBlob;
	std::wstring					 PDBName;
};

/*
	A DXIL library can be seen similarly as a regular DLL,
	which contains compiled code that can be accessed using a number of exported symbols.
	In the case of the raytracing pipeline, such symbols correspond to the names of the functions
	implementing the shader programs.
*/
class Library
{
public:
	Library() noexcept = default;
	Library(IDxcBlob* Blob, IDxcBlob* PDBBlob, std::wstring&& PDBName) noexcept
		: Blob(Blob)
		, PDBBlob(PDBBlob)
		, PDBName(std::move(PDBName))
	{
	}

	operator D3D12_SHADER_BYTECODE() const
	{
		return D3D12_SHADER_BYTECODE{ .pShaderBytecode = Blob->GetBufferPointer(),
									  .BytecodeLength  = Blob->GetBufferSize() };
	}

private:
	Microsoft::WRL::ComPtr<IDxcBlob> Blob;
	Microsoft::WRL::ComPtr<IDxcBlob> PDBBlob;
	std::wstring					 PDBName;
};

class ShaderCompiler
{
public:
	ShaderCompiler();

	void SetShaderModel(D3D_SHADER_MODEL ShaderModel);

	void SetIncludeDirectory(const std::filesystem::path& Path);

	Shader CompileShader(
		Shader::EType				  Type,
		const std::filesystem::path&  Path,
		std::wstring_view			  EntryPoint,
		const std::vector<DxcDefine>& ShaderDefines) const;

	Library CompileLibrary(const std::filesystem::path& Path) const;

private:
	std::wstring ShaderProfileString(Shader::EType Type, D3D_SHADER_MODEL ShaderModel) const;

	std::wstring LibraryProfileString(D3D_SHADER_MODEL ShaderModel) const;

	void Compile(
		const std::filesystem::path&  Path,
		std::wstring_view			  EntryPoint,
		std::wstring_view			  Profile,
		const std::vector<DxcDefine>& ShaderDefines,
		_Outptr_result_maybenull_ IDxcBlob** ppBlob,
		_Outptr_result_maybenull_ IDxcBlob** ppPDBBlob,
		std::wstring&						 PDBName) const;

private:
	Microsoft::WRL::ComPtr<IDxcCompiler3>	   Compiler3;
	Microsoft::WRL::ComPtr<IDxcUtils>		   Utils;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> DefaultIncludeHandler;
	D3D_SHADER_MODEL						   ShaderModel;
	std::wstring							   IncludeDirectory;
};
