#ifndef DMI_PARSER_HH
#define DMI_PARSER_HH

#include <cstring>
#include <stddef.h>
#include <stdint.h>

const int DMI_TYPE_BASEBOARD = 2;
const int DMI_TYPE_PROCESSOR = 4;
const int DMI_TYPE_MEMORY = 17;

#define SMBIOS_STRING( name )                                                                                                                        \
	uint8_t name##_;                                                                                                                                 \
	const char* name

namespace smbios
{

	// DMI_TYPE_BASEBOARD
	struct TypeBaseboard {
		// 2.0+
		SMBIOS_STRING(Manufacturer);
		SMBIOS_STRING(Product);
		SMBIOS_STRING(Version);
		SMBIOS_STRING(SerialNumber);
		SMBIOS_STRING(AssetTag);
		uint8_t FeatureFlags;
		SMBIOS_STRING(LocationInChassis);
		uint16_t ChassisHandle;
		uint8_t BoardType;
		uint8_t NoOfContainedObjectHandles;
		uint16_t* ContainedObjectHandles;
	};

	// DMI_TYPE_PROCESSOR
	struct TypeProcessor {
		// 2.0+
		SMBIOS_STRING(SocketDesignation);
		uint8_t ProcessorType;
		uint8_t ProcessorFamily;
		SMBIOS_STRING(ProcessorManufacturer);
		uint8_t ProcessorID[8];
		SMBIOS_STRING(ProcessorVersion);
		uint8_t Voltage;
		uint16_t ExternalClock;
		uint16_t MaxSpeed;
		uint16_t CurrentSpeed;
		uint8_t Status;
		uint8_t ProcessorUpgrade;
		// 2.1+
		uint16_t L1CacheHandle;
		uint16_t L2CacheHandle;
		uint16_t L3CacheHandle;
		// 2.3+
		SMBIOS_STRING(SerialNumber);
		SMBIOS_STRING(AssetTagNumber);
		SMBIOS_STRING(PartNumber);
		// 2.5+
		uint8_t CoreCount;
		uint8_t CoreEnabled;
		uint8_t ThreadCount;
		uint16_t ProcessorCharacteristics;
		// 2.6+
		uint16_t ProcessorFamily2;
		// 3.0+
		uint16_t CoreCount2;
		uint16_t CoreEnabled2;
		uint16_t ThreadCount2;
	};

	// DMI_TYPE_MEMORY
	struct TypeMemoryDevice {
		// 2.1+
		uint16_t PhysicalArrayHandle;
		uint16_t ErrorInformationHandle;
		uint16_t TotalWidth;
		uint16_t DataWidth;
		uint16_t Size;
		uint8_t FormFactor;
		uint8_t DeviceSet;
		SMBIOS_STRING(DeviceLocator);
		SMBIOS_STRING(BankLocator);
		uint8_t MemoryType;
		uint16_t TypeDetail;
		// 2.3+
		uint16_t Speed;
		SMBIOS_STRING(Manufacturer);
		SMBIOS_STRING(SerialNumber);
		SMBIOS_STRING(AssetTagNumber);
		SMBIOS_STRING(PartNumber);
		// 2.6+
		uint8_t Attributes;
		// 2.7+
		uint32_t ExtendedSize;
		uint16_t ConfiguredClockSpeed;
		// 2.8+
		uint16_t MinimumVoltage;
		uint16_t MaximumVoltage;
		uint16_t ConfiguredVoltage;
	};

	struct Entry {
		uint8_t type;
		uint8_t length;
		uint16_t handle;
		union {
			TypeProcessor processor;
			TypeBaseboard baseboard;
			TypeMemoryDevice memory;
		} data;
	};

	enum SpecVersion {
		SMBIOS_2_0 = 0x0200,
		SMBIOS_2_1 = 0x0201,
		SMBIOS_2_2 = 0x0202,
		SMBIOS_2_3 = 0x0203,
		SMBIOS_2_4 = 0x0204,
		SMBIOS_2_5 = 0x0205,
		SMBIOS_2_6 = 0x0206,
		SMBIOS_2_7 = 0x0207,
		SMBIOS_2_8 = 0x0208,
		SMBIOS_3_0 = 0x0300
	};

	class Parser
	{
	public:
		Parser(const uint8_t* data, size_t size, int version = 0);
		void reset();
		const Entry* next();
		int version() const;
		bool valid() const;

	private:
		const uint8_t* data_;
		size_t size_;
		Entry entry_;
		const uint8_t* ptr_;
		const uint8_t* start_;
		int version_;

		const Entry* parseEntry();
		const char* getString(int index) const;
	};

} // namespace smbios

#undef SMBIOS_STRING

#endif // DMI_PARSER_HH