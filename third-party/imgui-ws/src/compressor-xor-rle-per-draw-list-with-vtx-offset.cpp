/*! \file compressor-xor-rle-per-draw-list-with-vtx-offset.cpp
 *  \brief Enter description here.
 */

#include "imgui-ws/imgui-draw-data-compressor.h"

#include "imgui/imgui.h"

#include <cstring>

namespace {

void appendBytes(std::vector<char> & buf, const void * data, size_t n) {
    const size_t off = buf.size();
    buf.resize(off + n);
    std::memcpy(buf.data() + off, data, n);
}

void writeCmdListToBuffer(const ImDrawList * cmdList, std::vector<char> & buf) {
    buf.clear();

    const uint32_t nVertices = cmdList->VtxBuffer.Size;
    const uint32_t nIndicesOriginal = cmdList->IdxBuffer.Size;
    const uint32_t nIndices = nIndicesOriginal + (nIndicesOriginal % 2);
    const uint32_t nCmd = cmdList->CmdBuffer.Size;

    const size_t vtxBytes = (size_t) nVertices * sizeof(ImDrawVert);
    const size_t idxBytes = (size_t) nIndicesOriginal * sizeof(ImDrawIdx);
    const size_t cmdBytes = (size_t) nCmd * (4 * sizeof(uint32_t) + sizeof(ImVec4));

    buf.reserve(8 + sizeof(uint32_t) + vtxBytes + sizeof(uint32_t) + idxBytes + 2 + sizeof(uint32_t) + cmdBytes);

    float offsetX = cmdList->VtxBuffer.Data[0].pos.x;
    float offsetY = cmdList->VtxBuffer.Data[0].pos.y;
    appendBytes(buf, &offsetX, sizeof(offsetX));
    appendBytes(buf, &offsetY, sizeof(offsetY));
    appendBytes(buf, &nVertices, sizeof(nVertices));

    const size_t vtxOff = buf.size();
    buf.resize(vtxOff + vtxBytes);
    ImDrawVert * dstVtx = reinterpret_cast<ImDrawVert *>(buf.data() + vtxOff);
    for (uint32_t i = 0; i < nVertices; ++i) {
        dstVtx[i] = cmdList->VtxBuffer.Data[i];
        dstVtx[i].pos.x -= offsetX;
        dstVtx[i].pos.y -= offsetY;
    }

    appendBytes(buf, &nIndices, sizeof(nIndices));
    if (nIndicesOriginal > 0) {
        appendBytes(buf, cmdList->IdxBuffer.Data, idxBytes);
    }
    if (nIndicesOriginal % 2 == 1) {
        const uint16_t idx = 0;
        appendBytes(buf, &idx, sizeof(idx));
    }

    appendBytes(buf, &nCmd, sizeof(nCmd));

    for (uint32_t iCmd = 0; iCmd < nCmd; ++iCmd) {
        const ImDrawCmd * pcmd = &cmdList->CmdBuffer[iCmd];

        const uint32_t nElements = pcmd->ElemCount;
        appendBytes(buf, &nElements, sizeof(nElements));

        const uint32_t textureId = (uint32_t) (intptr_t) pcmd->TextureId;
        appendBytes(buf, &textureId, sizeof(textureId));

        const uint32_t offsetVtx = (uint32_t) (intptr_t) pcmd->VtxOffset;
        appendBytes(buf, &offsetVtx, sizeof(offsetVtx));

        const uint32_t offsetIdx = (uint32_t) (intptr_t) pcmd->IdxOffset;
        appendBytes(buf, &offsetIdx, sizeof(offsetIdx));

        appendBytes(buf, &pcmd->ClipRect, sizeof(ImVec4));
    }
}

}

namespace ImDrawDataCompressor {

struct XorRlePerDrawListWithVtxOffset::Impl {
};

XorRlePerDrawListWithVtxOffset::XorRlePerDrawListWithVtxOffset() : m_impl(new Impl()) {}

XorRlePerDrawListWithVtxOffset::~XorRlePerDrawListWithVtxOffset() {}

bool XorRlePerDrawListWithVtxOffset::setDrawData(const ::ImDrawData * drawData) {
    const uint32_t nCmdLists = drawData->CmdListsCount;
    m_drawListsCur.resize(nCmdLists);

    for (uint32_t iList = 0; iList < nCmdLists; ++iList) {
        ::writeCmdListToBuffer(drawData->CmdLists[iList], m_drawListsCur[iList]);
    }

    // XOR/RLE diff temporarily disabled — clients receive full draw lists.
    m_drawListsDiff.clear();

    return true;
}

}
