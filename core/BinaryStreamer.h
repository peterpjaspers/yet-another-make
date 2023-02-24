#pragma once

#include "IStreamer.h"
#include "IIOStream.h"
#include "IStreamable.h"
#include "IStreamableTypes.h"

#include <map>
#include <vector>

namespace YAM
{
    class __declspec(dllexport) BinaryWriter : public IStreamer
    {
    public:
        BinaryWriter(IOutputStream* oStream);
        BinaryWriter(IStreamableTypes* types, IOutputStream* oStream);

        bool writing() const override { return true; }
        void stream(void* bytes, unsigned int nBytes) override;
        void stream(bool& value) override;
        void stream(float& value) override;
        void stream(double& value) override;
        void stream(int8_t& value) override;
        void stream(uint8_t& value) override;
        void stream(int16_t& value) override;
        void stream(uint16_t& value) override;
        void stream(int32_t& value) override;
        void stream(uint32_t& value) override;
        void stream(int64_t& value) override;
        void stream(uint64_t& value) override;

        void stream(std::string&) override;
        void stream(std::wstring&) override;


        void stream(IStreamable** streamable) override;
        void stream(std::shared_ptr<IStreamable>& streamable) override;

        bool eos() override;
        void flush() override;

    private:
        IStreamableTypes* _types;
        IOutputStream* _stream;
        std::map<IStreamable*, int> _objects;
    };
}

namespace YAM
{
    class __declspec(dllexport) BinaryReader : public IStreamer
    {
    public:
        BinaryReader(IInputStream* iStream);
        BinaryReader(IStreamableTypes* types, IInputStream* iStream);

        bool writing() const override { return false; }
        void stream(void* bytes, unsigned int nBytes) override;
        void stream(bool& value) override;
        void stream(float& value) override;
        void stream(double& value) override;
        void stream(int8_t& value) override;
        void stream(uint8_t& value) override;
        void stream(int16_t& value) override;
        void stream(uint16_t& value) override;
        void stream(int32_t& value) override;
        void stream(uint32_t& value) override;
        void stream(int64_t& value) override;
        void stream(uint64_t& value) override;

        void stream(std::string&) override;
        void stream(std::wstring&) override;

        void stream(IStreamable** streamable) override;
        void stream(std::shared_ptr<IStreamable>& streamable) override;

        bool eos() override;
        void flush() override;

    private:
        IStreamableTypes* _types;
        IInputStream* _stream;
        std::vector<IStreamable*> _objects;
        std::map<IStreamable*, std::shared_ptr<IStreamable> > _sharedObjects;
    };
}
