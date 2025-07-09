#pragma once
#include "GfxDescriptor.h"
#include "GfxResourceCommon.h"

namespace adria
{
    enum class GfxLoadAccessOp : Uint8
    {
        Discard,
        Preserve,
        Clear,
        NoAccess
    };

	enum class GfxStoreAccessOp : Uint8
	{
		Discard,
		Preserve,
		Resolve,
		NoAccess
	};

    enum GfxRenderPassFlagBit : Uint32
    {
        GfxRenderPassFlagBit_None = 0,
        GfxRenderPassFlagBit_ReadOnlyDepth = BIT(0),
        GfxRenderPassFlagBit_ReadOnlyStencil = BIT(1),
        GfxRenderPassFlagBit_AllowUAVWrites = BIT(2),
        GfxRenderPassFlagBit_SuspendingPass = BIT(3),
        GfxRenderPassFlagBit_ResumingPass = BIT(4),
    };
    ENABLE_ENUM_BIT_OPERATORS(GfxRenderPassFlagBit);
    using GfxRenderPassFlags = Uint32;

    struct GfxColorAttachmentDesc
    {
        GfxDescriptor cpu_handle;
        GfxLoadAccessOp beginning_access;
        GfxStoreAccessOp ending_access;
        GfxClearValue clear_value;
    };

    struct GfxDepthAttachmentDesc
    {
        GfxDescriptor       cpu_handle;
        GfxLoadAccessOp     depth_beginning_access;
        GfxStoreAccessOp    depth_ending_access;
        GfxLoadAccessOp     stencil_beginning_access = GfxLoadAccessOp::NoAccess;
        GfxStoreAccessOp    stencil_ending_access = GfxStoreAccessOp::NoAccess;
        GfxClearValue       clear_value;
    };

    struct GfxRenderPassDesc
    {
        std::vector<GfxColorAttachmentDesc> rtv_attachments{};
        std::optional<GfxDepthAttachmentDesc> dsv_attachment = std::nullopt;
        GfxRenderPassFlags flags = GfxRenderPassFlagBit_None;
        Uint32 width = 0;
        Uint32 height = 0;
        Bool legacy = false;
    };
}