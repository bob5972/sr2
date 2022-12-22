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

        FloatNet(uint numInputs, uint numOutputs, uint numNodes)
        :myInitialized(FALSE)
        {
            initialize(numInputs, numOutputs, numNodes);
        }

        void initialize(uint numInputs, uint numOutputs, uint numNodes);

        void compute(const MBVector<float> &inputs, MBVector<float> &outputs);

        void load(MBRegistry *mreg, const char *prefix);
        void loadZeroNet();
        void mutate(float rate, uint maxNodeDegree, uint maxNodes);
        void save(MBRegistry *mreg, const char *prefix);

        uint minimize(CPBitVector *inputBV);

        uint getNumInputs()  { return myNumInputs;    }
        uint getNumOutputs() { return myNumOutputs;   }
        uint getNumNodes()   { return myNodes.size(); }

        void voidOutputNode(uint i) {
            uint numNodes = myNodes.size();
            ASSERT(numNodes >= myNumOutputs);
            i = i + numNodes - myNumOutputs;
            myNodes[i].makeVoid();
        }

    private:
        bool myInitialized;
        uint myNumInputs;
        uint myNumOutputs;

        /*
         * Inputs have values but not nodes.
         * Inner nodes and Outputs have both.
         */
        MBVector<MLFloatNode> myNodes;
        MBVector<float> myValues;

        // void nodeShiftHelper(uint index, bool shiftUp);
};

#endif // _FLOATNET_H_202208121158