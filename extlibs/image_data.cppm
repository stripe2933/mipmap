module;

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

export module image_data;
import std;

template <typename T, typename... Ts>
concept one_of = (std::same_as<T, Ts> || ...);

export template <one_of<stbi_uc, stbi_us, float> T>
class ImageData {
public:
    int width, height, channels;
    std::unique_ptr<T[]> data;

    template <typename U>
    explicit ImageData(
        std::span<const U> memory,
        int desiredChannels = 0
    ) {
        constexpr auto loadFunc = [] consteval {
            if constexpr (std::same_as<T, stbi_uc>) {
                return &stbi_load_from_memory;
            }
            if constexpr (std::same_as<T, stbi_us>) {
                return &stbi_load_16_from_memory;
            }
            if constexpr (std::same_as<T, float>) {
                return &stbi_loadf_from_memory;
            }
        }();

        data.reset(loadFunc(
            reinterpret_cast<const stbi_uc*>(memory.data()), memory.size_bytes(),
            &width, &height, &channels, desiredChannels));
        if (desiredChannels != 0){
            channels = desiredChannels;
        }
        checkError();
    }

    explicit ImageData(
        const std::filesystem::path &path,
        int desiredChannels = 0
    ) {
        constexpr auto loadFunc = [] consteval {
            if constexpr (std::same_as<T, stbi_uc>) {
                return &stbi_load;
            }
            if constexpr (std::same_as<T, stbi_us>) {
                return &stbi_load_16;
            }
            if constexpr (std::same_as<T, float>) {
                return &stbi_loadf;
            }
        }();

        data.reset(loadFunc(
            path.c_str(),
            &width, &height, &channels, desiredChannels));
        if (desiredChannels != 0){
            channels = desiredChannels;
        }
        checkError();
    }

private:
    auto checkError() const -> void {
        if (!data) {
            throw std::runtime_error { std::format("Failed to load image: {}", stbi_failure_reason()) };
        }
    }
};