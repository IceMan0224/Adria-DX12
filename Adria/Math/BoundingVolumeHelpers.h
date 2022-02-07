#pragma once
#include <DirectXCollision.h>
#include <vector>
#include <concepts>
#include <algorithm>

namespace adria
{
   
    template<typename V>
    concept HasPosition = requires (V v)
    {
        {v.position.x} -> std::convertible_to<float32>;
        {v.position.y} -> std::convertible_to<float32>;
        {v.position.z} -> std::convertible_to<float32>;
    };

    template<typename Iterator> requires HasPosition<std::iter_value_t<Iterator>>
    DirectX::BoundingBox AABBFromRange(Iterator begin, Iterator end)
    {
        using V = typename std::iterator_traits<Iterator>::value_type;

        auto xExtremes = std::minmax_element(begin, end,
            [](V const& lhs, V const& rhs) {
                return lhs.position.x < rhs.position.x;
            });

        auto yExtremes = std::minmax_element(begin, end,
            [](V const& lhs, V const& rhs) {
                return lhs.position.y < rhs.position.y;
            });

        auto zExtremes = std::minmax_element(begin, end,
            [](V const& lhs, V const& rhs) {
                return lhs.position.z < rhs.position.z;
            });

        DirectX::XMFLOAT3 lowerLeft(xExtremes.first->position.x, yExtremes.first->position.y, zExtremes.first->position.z);
        DirectX::XMFLOAT3 upperRight(xExtremes.second->position.x, yExtremes.second->position.y, zExtremes.second->position.z);

        DirectX::XMFLOAT3 center((lowerLeft.x + upperRight.x) * 0.5f, (lowerLeft.y + upperRight.y) * 0.5f, (lowerLeft.z + upperRight.z) * 0.5f);
        DirectX::XMFLOAT3 extents(upperRight.x - center.x, upperRight.y - center.y, upperRight.z - center.z);

        return DirectX::BoundingBox(center, extents);
    }

    template<typename V>
    DirectX::BoundingBox AABBFromVertices(std::vector<V> const& vertices)
    {
        return AABBFromRange(vertices.begin(), vertices.end());
    }
}