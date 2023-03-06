//------------------------------------------------------------------------------
// Copyright (C)    : Philips Medical Systems Nederland B.V. 2018
// Description      :
//------------------------------------------------------------------------------

#ifndef ENDIANSHIP_H
#define ENDIANSHIP_H

namespace dclplatform
    {
        class EndianShip
        {
        public:
            EndianShip()
            {
                union
                {
                    uint32_t i;
                    char c[4];
                } e = { 0x01000000 };
                mIsBigEndian = (e.c[0] != 0);
            }

            inline bool IsBigEndian() const
            {
                return mIsBigEndian;
            }

        private:
            bool mIsBigEndian;
        };
}
#endif