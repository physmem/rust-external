#include "smbios.hpp"

#define DMI_READ_8U           *ptr_++
#define DMI_READ_16U          *( ( uint16_t* )ptr_ ), ptr_ += 2
#define DMI_READ_32U          *( ( uint32_t* )ptr_ ), ptr_ += 4
#define DMI_READ_64U          *( ( uint64_t* )ptr_ ), ptr_ += 8
#define DMI_ENTRY_HEADER_SIZE 4

namespace smbios
{

#ifdef _WIN32
#	define WIN32_LEAN_AND_MEAN
#	include <Windows.h>

	struct RawSMBIOSData {
		uint8_t Used20CallingMethod;
		uint8_t SMBIOSMajorVersion;
		uint8_t SMBIOSMinorVersion;
		uint8_t DmiRevision;
		uint32_t Length;
		uint8_t SMBIOSTableData[1];
	};
#endif

	Parser::Parser(const uint8_t* data, size_t size, int version) : data_(data + 32), size_(size), ptr_(NULL), version_(version)
	{
		// VM_FISH_BLACK_START;

		int vn = 0;

		RawSMBIOSData* smBiosData = NULL;
		smBiosData = (RawSMBIOSData*)data;

		// get the SMBIOS version
		vn = smBiosData->SMBIOSMajorVersion << 8 | smBiosData->SMBIOSMinorVersion;
		data_ = smBiosData->SMBIOSTableData;
		size_ = smBiosData->Length;

		if (version_ == 0)
			version_ = SMBIOS_3_0;
		if (version_ > vn)
			version_ = vn;
		// is a valid version?
		if ((version_ < SMBIOS_2_0 || version_ > SMBIOS_2_8) && version_ != SMBIOS_3_0)
			goto INVALID_DATA;
		reset();
		return;

	INVALID_DATA:
		data_ = ptr_ = start_ = NULL;

		// VM_FISH_BLACK_END;
	}

	const char* Parser::getString(int index) const
	{
		// VM_FISH_BLACK_START;

		if (index <= 0)
			return "";

		const char* ptr = (const char*)start_ + (size_t)entry_.length - DMI_ENTRY_HEADER_SIZE;
		for (int i = 1; *ptr != 0 && i < index; ++i) {
			// TODO: check buffer limits
			while (*ptr != 0)
				++ptr;
			++ptr;
		}

		// VM_FISH_BLACK_END;
		return ptr;
	}

	void Parser::reset()
	{
		ptr_ = start_ = NULL;
	}

	const Entry* Parser::next()
	{
		if (data_ == NULL)
			return NULL;

		// jump to the next field
		if (ptr_ == NULL)
			ptr_ = start_ = data_;
		else {
			ptr_ = start_ + entry_.length - DMI_ENTRY_HEADER_SIZE;
			while (ptr_ < data_ + size_ - 1 && !(ptr_[0] == 0 && ptr_[1] == 0))
				++ptr_;
			ptr_ += 2;
			if (ptr_ >= data_ + size_) {
				ptr_ = start_ = NULL;
				return NULL;
			}
		}

		memset(&entry_, 0, sizeof(entry_));

		// entry header
		entry_.type = DMI_READ_8U;
		entry_.length = DMI_READ_8U;
		entry_.handle = DMI_READ_16U;
		start_ = ptr_;

		if (entry_.type == 127) {
			reset();
			return NULL;
		}
		auto entry = parseEntry();

		return entry;
	}

	const Entry* Parser::parseEntry()
	{
		if (entry_.type == DMI_TYPE_BASEBOARD) {
			// 2.0+
			if (version_ >= SMBIOS_2_0) {
				entry_.data.baseboard.Manufacturer_ = DMI_READ_8U;
				entry_.data.baseboard.Product_ = DMI_READ_8U;
				entry_.data.baseboard.Version_ = DMI_READ_8U;
				entry_.data.baseboard.SerialNumber_ = DMI_READ_8U;
				entry_.data.baseboard.AssetTag_ = DMI_READ_8U;
				entry_.data.baseboard.FeatureFlags = DMI_READ_8U;
				entry_.data.baseboard.LocationInChassis_ = DMI_READ_8U;
				entry_.data.baseboard.ChassisHandle = DMI_READ_16U;
				entry_.data.baseboard.BoardType = DMI_READ_8U;
				entry_.data.baseboard.NoOfContainedObjectHandles = DMI_READ_8U;
				entry_.data.baseboard.ContainedObjectHandles = (uint16_t*)ptr_;
				ptr_ += entry_.data.baseboard.NoOfContainedObjectHandles * sizeof(uint16_t);

				entry_.data.baseboard.Manufacturer = getString(entry_.data.baseboard.Manufacturer_);
				entry_.data.baseboard.Product = getString(entry_.data.baseboard.Product_);
				entry_.data.baseboard.Version = getString(entry_.data.baseboard.Version_);
				entry_.data.baseboard.SerialNumber = getString(entry_.data.baseboard.SerialNumber_);
				entry_.data.baseboard.AssetTag = getString(entry_.data.baseboard.AssetTag_);
				entry_.data.baseboard.LocationInChassis = getString(entry_.data.baseboard.LocationInChassis_);
			}

			return &entry_;
		}
		else if (entry_.type == DMI_TYPE_PROCESSOR) {
			// 2.0+
			if (version_ >= smbios::SMBIOS_2_0) {
				entry_.data.processor.SocketDesignation_ = DMI_READ_8U;
				entry_.data.processor.ProcessorType = DMI_READ_8U;
				entry_.data.processor.ProcessorFamily = DMI_READ_8U;
				entry_.data.processor.ProcessorManufacturer_ = DMI_READ_8U;
				for (int i = 0; i < 8; ++i)
					entry_.data.processor.ProcessorID[i] = DMI_READ_8U;
				entry_.data.processor.ProcessorVersion_ = DMI_READ_8U;
				entry_.data.processor.Voltage = DMI_READ_8U;
				entry_.data.processor.ExternalClock = DMI_READ_16U;
				entry_.data.processor.MaxSpeed = DMI_READ_16U;
				entry_.data.processor.CurrentSpeed = DMI_READ_16U;
				entry_.data.processor.Status = DMI_READ_8U;
				entry_.data.processor.ProcessorUpgrade = DMI_READ_8U;

				entry_.data.processor.SocketDesignation = getString(entry_.data.processor.SocketDesignation_);
				entry_.data.processor.ProcessorManufacturer = getString(entry_.data.processor.ProcessorManufacturer_);
				entry_.data.processor.ProcessorVersion = getString(entry_.data.processor.ProcessorVersion_);
			}
			// 2.1+
			if (version_ >= smbios::SMBIOS_2_1) {
				entry_.data.processor.L1CacheHandle = DMI_READ_16U;
				entry_.data.processor.L2CacheHandle = DMI_READ_16U;
				entry_.data.processor.L3CacheHandle = DMI_READ_16U;
			}
			// 2.3+
			if (version_ >= smbios::SMBIOS_2_3) {
				entry_.data.processor.SerialNumber_ = DMI_READ_8U;
				entry_.data.processor.AssetTagNumber_ = DMI_READ_8U;
				entry_.data.processor.PartNumber_ = DMI_READ_8U;

				entry_.data.processor.SerialNumber = getString(entry_.data.processor.SerialNumber_);
				entry_.data.processor.AssetTagNumber = getString(entry_.data.processor.AssetTagNumber_);
				entry_.data.processor.PartNumber = getString(entry_.data.processor.PartNumber_);
			}
			// 2.5+
			if (version_ >= smbios::SMBIOS_2_5) {
				entry_.data.processor.CoreCount = DMI_READ_8U;
				entry_.data.processor.CoreEnabled = DMI_READ_8U;
				entry_.data.processor.ThreadCount = DMI_READ_8U;
				entry_.data.processor.ProcessorCharacteristics = DMI_READ_16U;
			}
			// 2.6+
			if (version_ >= smbios::SMBIOS_2_6) {
				entry_.data.processor.ProcessorFamily2 = DMI_READ_16U;
			}
			// 3.0+
			if (version_ >= smbios::SMBIOS_3_0) {
				entry_.data.processor.CoreCount2 = DMI_READ_16U;
				entry_.data.processor.CoreEnabled2 = DMI_READ_16U;
				entry_.data.processor.ThreadCount2 = DMI_READ_16U;
			}

			return &entry_;
		}
		else if (entry_.type == DMI_TYPE_MEMORY) {
			// 2.1+
			if (version_ >= smbios::SMBIOS_2_1) {
				entry_.data.memory.PhysicalArrayHandle = DMI_READ_16U;
				entry_.data.memory.ErrorInformationHandle = DMI_READ_16U;
				entry_.data.memory.TotalWidth = DMI_READ_16U;
				entry_.data.memory.DataWidth = DMI_READ_16U;
				entry_.data.memory.Size = DMI_READ_16U;
				entry_.data.memory.FormFactor = DMI_READ_8U;
				entry_.data.memory.DeviceSet = DMI_READ_8U;
				entry_.data.memory.DeviceLocator_ = DMI_READ_8U;
				entry_.data.memory.BankLocator_ = DMI_READ_8U;
				entry_.data.memory.MemoryType = DMI_READ_8U;
				entry_.data.memory.TypeDetail = DMI_READ_16U;

				entry_.data.memory.DeviceLocator = getString(entry_.data.memory.DeviceLocator_);
				entry_.data.memory.BankLocator = getString(entry_.data.memory.BankLocator_);
			}
			// 2.3+
			if (version_ >= smbios::SMBIOS_2_3) {
				entry_.data.memory.Speed = DMI_READ_16U;
				entry_.data.memory.Manufacturer_ = DMI_READ_8U;
				entry_.data.memory.SerialNumber_ = DMI_READ_8U;
				entry_.data.memory.AssetTagNumber_ = DMI_READ_8U;
				entry_.data.memory.PartNumber_ = DMI_READ_8U;

				entry_.data.memory.Manufacturer = getString(entry_.data.memory.Manufacturer_);
				entry_.data.memory.SerialNumber = getString(entry_.data.memory.SerialNumber_);
				entry_.data.memory.AssetTagNumber = getString(entry_.data.memory.AssetTagNumber_);
				entry_.data.memory.PartNumber = getString(entry_.data.memory.PartNumber_);
			}
			// 2.6+
			if (version_ >= smbios::SMBIOS_2_6) {
				entry_.data.memory.Attributes = DMI_READ_8U;
			}
			// 2.7+
			if (version_ >= smbios::SMBIOS_2_7) {
				entry_.data.memory.ExtendedSize = DMI_READ_32U;
				entry_.data.memory.ConfiguredClockSpeed = DMI_READ_16U;
			}
			// 2.8+
			if (version_ >= smbios::SMBIOS_2_8) {
				entry_.data.memory.MinimumVoltage = DMI_READ_16U;
				entry_.data.memory.MinimumVoltage = DMI_READ_16U;
				entry_.data.memory.ConfiguredVoltage = DMI_READ_16U;
			}
		}

		auto result = &entry_;

		return result;
	}

	int Parser::version() const
	{
		return version_;
	}

	bool Parser::valid() const
	{
		return data_ != NULL;
	}

} // namespace smbios