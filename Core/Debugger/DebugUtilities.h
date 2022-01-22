#pragma once
#include "stdafx.h"
#include "Core/Debugger/DebugTypes.h"
#include "Core/SnesMemoryType.h"

class DebugUtilities
{
public:
	static constexpr SnesMemoryType GetCpuMemoryType(CpuType type)
	{
		switch(type) {
			case CpuType::Snes: return SnesMemoryType::CpuMemory;
			case CpuType::Spc: return SnesMemoryType::SpcMemory;
			case CpuType::NecDsp: return SnesMemoryType::NecDspMemory;
			case CpuType::Sa1: return SnesMemoryType::Sa1Memory;
			case CpuType::Gsu: return SnesMemoryType::GsuMemory;
			case CpuType::Cx4: return SnesMemoryType::Cx4Memory;
			case CpuType::Gameboy: return SnesMemoryType::GameboyMemory;
			case CpuType::Nes: return SnesMemoryType::NesMemory;
		}

		throw std::runtime_error("Invalid CPU type");
	}

	static constexpr int GetProgramCounterSize(CpuType type)
	{
		switch(type) {
			case CpuType::Snes: return 6;
			case CpuType::Spc: return 4;
			case CpuType::NecDsp: return 6;
			case CpuType::Sa1: return 6;
			case CpuType::Gsu: return 6;
			case CpuType::Cx4: return 6;
			case CpuType::Gameboy: return 4;
			case CpuType::Nes: return 4;
		}

		throw std::runtime_error("Invalid CPU type");
	}

	static constexpr CpuType ToCpuType(SnesMemoryType type)
	{
		switch(type) {
			case SnesMemoryType::SpcMemory:
			case SnesMemoryType::SpcRam:
			case SnesMemoryType::SpcRom:
				return CpuType::Spc;

			case SnesMemoryType::GsuMemory:
			case SnesMemoryType::GsuWorkRam:
				return CpuType::Gsu;

			case SnesMemoryType::Sa1InternalRam:
			case SnesMemoryType::Sa1Memory:
				return CpuType::Sa1;

			case SnesMemoryType::DspDataRam:
			case SnesMemoryType::DspDataRom:
			case SnesMemoryType::DspProgramRom:
				return CpuType::NecDsp;

			case SnesMemoryType::Cx4DataRam:
			case SnesMemoryType::Cx4Memory:
				return CpuType::Cx4;
				
			case SnesMemoryType::GbPrgRom:
			case SnesMemoryType::GbWorkRam:
			case SnesMemoryType::GbCartRam:
			case SnesMemoryType::GbHighRam:
			case SnesMemoryType::GbBootRom:
			case SnesMemoryType::GbVideoRam:
			case SnesMemoryType::GbSpriteRam:
			case SnesMemoryType::GameboyMemory:
				return CpuType::Gameboy;

			case SnesMemoryType::NesChrRam:
			case SnesMemoryType::NesChrRom:
			case SnesMemoryType::NesInternalRam:
			case SnesMemoryType::NesMemory:
			case SnesMemoryType::NesNametableRam:
			case SnesMemoryType::NesPaletteRam:
			case SnesMemoryType::NesPpuMemory:
			case SnesMemoryType::NesPrgRom:
			case SnesMemoryType::NesSaveRam:
			case SnesMemoryType::NesSpriteRam:
			case SnesMemoryType::NesSecondarySpriteRam:
			case SnesMemoryType::NesWorkRam:
				return CpuType::Nes;

			default:
				return CpuType::Snes;
		}

		throw std::runtime_error("Invalid CPU type");
	}

	static constexpr bool IsRelativeMemory(SnesMemoryType memType)
	{
		return memType <= GetLastCpuMemoryType();
	}

	static constexpr SnesMemoryType GetLastCpuMemoryType()
	{
		//TODO refactor to "IsRelativeMemory"?
		return SnesMemoryType::NesPpuMemory;
	}

	static constexpr bool IsPpuMemory(SnesMemoryType memType)
	{
		switch(memType) {
			case SnesMemoryType::VideoRam:
			case SnesMemoryType::SpriteRam:
			case SnesMemoryType::CGRam:
			case SnesMemoryType::GbVideoRam:
			case SnesMemoryType::GbSpriteRam:
			
			case SnesMemoryType::NesChrRam:
			case SnesMemoryType::NesChrRom:
			case SnesMemoryType::NesSpriteRam:
			case SnesMemoryType::NesPaletteRam:
			case SnesMemoryType::NesNametableRam:
			case SnesMemoryType::NesSecondarySpriteRam:
			case SnesMemoryType::NesPpuMemory:
				return true;

			default: 
				return false;
		}
	}

	static constexpr bool IsRomMemory(SnesMemoryType memType)
	{
		switch(memType) {
			case SnesMemoryType::PrgRom:
			case SnesMemoryType::GbPrgRom:
			case SnesMemoryType::GbBootRom:
			case SnesMemoryType::NesPrgRom:
			case SnesMemoryType::NesChrRom:
			case SnesMemoryType::SaveRam: //Include save ram here to avoid uninit memory read warnings on save ram
				return true;

			default:
				return false;
		}
	}

	static constexpr CpuType GetLastCpuType()
	{
		return CpuType::Nes;
	}
};