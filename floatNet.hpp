/*
 * floatNet.hpp -- part of SpaceRobots2
 * Copyright (C) 2022 Michael Banack <github@banack.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _FLOATNET_H_202208121158
#define _FLOATNET_H_202208121158

#include "MBTypes.h"
#include "MBVector.hpp"
#include "MBRegistry.h"
#include "ml.hpp"
#include "BitVector.hpp"

class FloatNet
{
    public:
        FloatNet()
        :myInitialized(FALSE)
        {}

        FloatNet(uint numInputs, uint numOutputs, uint numInnerNodes)
        :myInitialized(FALSE)
        {
            initialize(numInputs, numOutputs, numInnerNodes);
        }

        void initialize(uint numInputs, uint numOutputs, uint numInnerNodes);

        void compute(const MBVector<float> &inputs, MBVector<float> &outputs);

        void load(MBRegistry *mreg, const char *prefix);
        void loadZeroNet();
        void mutate(float rate, uint maxNodeDegree, uint maxNodes);
        void save(MBRegistry *mreg, const char *prefix);

        uint minimize();

        void getUsedInputs(CPBitVector &inputBV) {
            inputBV = myUsedInputs;
        }
        void getUsedOutputs(CPBitVector &outputBV) {
            outputBV = myUsedOutputs;
        }

        uint getNumInputs()  { return myNumInputs;  }
        uint getNumOutputs() { return myNumOutputs; }
        uint getNumNodes()   { return myNumNodes;   }

        uint getOutputOffset() {
            NOT_IMPLEMENTED();//XXX bob5972
            return myNumInputs + myNodes.size() - myNumOutputs;
        }

        void voidOutputNode(uint i) {
            ASSERT(i >= 0 && i <= myNumOutputs);
            myUsedOutputs.reset(i);
        }

    private:
        bool myInitialized;
        bool myHaveOutputOrdering;

        CPBitVector myUsedInputs;
        CPBitVector myUsedOutputs;

        uint myNumInputs;
        uint myNumOutputs;
        uint myNumNodes;

        MBVector<MLFloatNode> myNodes;
        MBVector<uint> myOutputOrdering;
        MBVector<float> myValues;
};

#endif // _FLOATNET_H_202208121158