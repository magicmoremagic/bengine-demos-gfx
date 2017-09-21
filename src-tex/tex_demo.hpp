#pragma once
#ifndef TEX_DEMO_HPP_
#define TEX_DEMO_HPP_

#include <be/core/lifecycle.hpp>
#include <be/core/glm.hpp>
#include <be/core/time.hpp>
#include <be/core/filesystem.hpp>
#include <be/util/xoroshiro_128_plus.hpp>
#include <be/platform/lifecycle.hpp>
#include <be/platform/glfw.hpp>
#include <be/gfx/tex/texture.hpp>
#include <be/gfx/bgl.hpp>
#include <functional>
#include <random>

///////////////////////////////////////////////////////////////////////////////
class TexDemo final {
public:
   TexDemo(int argc, char** argv);
   int operator()();

private:
   void run_();
   void upload_();

   be::CoreInitLifecycle init_;
   be::CoreLifecycle core_;
   be::PlatformLifecycle platform_;

   be::I8 status_ = 0;

   bool resizable_ = false;
   be::ivec2 dim_ = be::ivec2(160, 120);
   be::F32 scale_ = 4.f;
   bool linear_scaling_ = false;
   GLFWwindow* wnd_ = nullptr;
   be::gfx::tex::ImageFormat format_; // TODO
   be::gfx::tex::Texture tex_;
   be::gfx::gl::GLuint tex_id_ = 0;
   std::function<void()> setup_;
   std::function<void()> generator_;
   bool animate_ = false;
   be::util::xo128p rnd_;
   std::uniform_int_distribution<> idist_ = std::uniform_int_distribution<>(0, 255);
   std::uniform_real_distribution<be::F32> fdist_ = std::uniform_real_distribution<be::F32>(0.f, 1.f);
   be::TU last_ = be::TU::zero();
   be::TU now_ = be::TU::zero();
   be::F64 time_ = 0.0;
   be::F32 time_scale_ = 10.f;
   be::F32 sin_time_ = 0.f;
   be::F32 effect_scale_ = 1.f;
   be::vec4 data_[8];
   be::Path file_;
};

#endif
