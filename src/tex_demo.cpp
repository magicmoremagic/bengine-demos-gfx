#include "tex_demo.hpp"
#include <be/core/logging.hpp>
#include <be/core/version.hpp>
#include <be/core/stack_trace.hpp>
#include <be/core/alg.hpp>
#include <be/gfx/version.hpp>
#include <be/gfx/tex/pixel_access_norm.hpp>
#include <be/gfx/tex/make_texture.hpp>
#include <be/gfx/tex/visit_texture.hpp>
#include <be/gfx/tex/convert_colorspace_static.hpp>
#include <be/cli/cli.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/common.hpp>
#include <glbinding/Binding.h>
#include <sstream>
#include <string>

using namespace gl;
using namespace be;
using namespace be::gfx;
using namespace be::gfx::tex;

///////////////////////////////////////////////////////////////////////////////
TexDemo::TexDemo(int argc, char** argv) {
   default_log().verbosity_mask(v::info_or_worse);
   try {
      using namespace cli;
      using namespace color;
      using namespace ct;
      Processor proc;

      bool show_version = false;
      bool show_help = false;
      S help_query;

      format_ = ImageFormat(U8(4), U8(1), BlockPacking::s_8_8_8_8, 4,
                            component_types(ComponentType::unorm, 4),
                            swizzles_rgba(), Colorspace::srgb, true);

      proc
         (prologue(Table() << header << "be::gfx::tex Demo").query())
         (synopsis(Cell() << fg_dark_gray << "[ " << fg_cyan << "OPTIONS" << fg_dark_gray << " ] " << fg_cyan << "DEMO"))

         (summary(Table() << header << "Demo Name" << cell << "Description"
                          << row << "ub" << cell << "Displays uninitialized texture data.  Technically this is undefined behavior."
                          << row << "whitenoise" << cell << "."
                          << row << "rgbnoise" << cell << "."
                          << row << "gradient" << cell << "Draws a gradient on the texture with random colors at each corner and interpolates linearly between them."
                          << row << "sinc" << cell << "Draws a gray field, perturbed towards white and black based on a sinc filter.  Use " << fg_yellow << "--effect-scale" << reset << " to change the 'zoom' factor."
                          << row << "cosdst2" << cell << "Draws a texture where each pixel's value is the cosine of the square of its distance form the center of the texture.  Use " << fg_yellow << "--effect-scale" << reset << " to change the 'zoom' factor."
                          << row << "pinwheel" << cell << "Draws a texture where each pixel's hue is determined by its angle around the center.  Use " << fg_yellow << "--effect-scale" << reset << " to change the frequency of color change."
                          << row << "pinwheel-r" << cell << "Like pinwheel, but discards the blue/green channels and uses red instead."
         ))

         (any(
            [this](const S& arg) {
               S demo = arg;
               std::transform(demo.begin(), demo.end(), demo.begin(), [](char c) { return (char)tolower(c); });
               if (demo == "ub") {
                  generator_ = []() { };
               } else if (demo == "whitenoise") {
                  generator_ = [this]() {
                     auto put = put_pixel_norm_func<ivec2>(tex_.view.image());
                     visit_texture_pixels<ivec2>(tex_.view, [=](ImageView& view, ivec2 pc) {
                        F32 val = fdist_(rnd_);
                        vec4 pixel_norm = vec4(val);
                        put(view, pc, pixel_norm);
                     });
                  };
               } else if (demo == "rgbnoise") {
                  generator_ = [this]() {
                     auto put = put_pixel_norm_func<ivec2>(tex_.view.image());
                     visit_texture_pixels<ivec2>(tex_.view, [=](ImageView& view, ivec2 pc) {
                        vec4 pixel_norm = vec4(fdist_(rnd_), fdist_(rnd_), fdist_(rnd_), 1.f);
                        put(view, pc, pixel_norm);
                     });
                  };
               } else if (demo == "gradient") {
                  generator_ = [this]() {
                     vec4 a = vec4(fdist_(rnd_), fdist_(rnd_), fdist_(rnd_), 1.f);
                     vec4 b = vec4(fdist_(rnd_), fdist_(rnd_), fdist_(rnd_), 1.f);
                     vec4 c = vec4(fdist_(rnd_), fdist_(rnd_), fdist_(rnd_), 1.f);
                     vec4 d = vec4(fdist_(rnd_), fdist_(rnd_), fdist_(rnd_), 1.f);
                     auto put = put_pixel_norm_func<ivec2>(tex_.view.image());
                     visit_texture_pixels<ivec2>(tex_.view, [=](ImageView& view, ivec2 pc) {
                        vec4 ab = glm::mix(a, b, (pc.x + 0.5f) / view.dim().x);
                        vec4 cd = glm::mix(c, d, (pc.x + 0.5f) / view.dim().x);
                        vec4 pixel_norm = glm::mix(ab, cd, (pc.y + 0.5f) / view.dim().y);
                        put(view, pc, pixel_norm);
                     });
                  };
               } else if (demo == "sinc") {
                  generator_ = [this]() {
                     auto put = put_pixel_norm_func<ivec2>(tex_.view.image());
                     visit_texture_pixels<ivec2>(tex_.view, [=](ImageView& view, ivec2 pc) {
                        vec2 center = vec2(view.dim()) / 2.f;
                        vec2 pixel_center = vec2(pc) + 0.5f;
                        vec2 offset = pixel_center - center;
                        F32 dst = glm::length(offset);
                        F32 val = 0.5f * (1.f + sinf(dst / effect_scale_) / (dst / effect_scale_));
                        vec4 pixel_norm = vec4(val);
                        put(view, pc, pixel_norm);
                     });
                  };
               } else if (demo == "cosdst2") {
                  generator_ = [this]() {
                     auto put = put_pixel_norm_func<ivec2>(tex_.view.image());
                     visit_texture_pixels<ivec2>(tex_.view, [=](ImageView& view, ivec2 pc) {
                        vec2 center = vec2(view.dim()) / 2.f;
                        vec2 pixel_center = vec2(pc) + 0.5f;
                        vec2 offset = pixel_center - center;
                        F32 dst2 = glm::length2(offset);
                        F32 val = 0.5f * (1.f + cosf(dst2 / effect_scale_));
                        vec4 pixel_norm = vec4(val);
                        put(view, pc, pixel_norm);
                     });
                  };
               } else if (demo == "pinwheel") {
                  generator_ = [this]() {
                     auto put = put_pixel_norm_func<ivec2>(tex_.view.image());
                     visit_texture_pixels<ivec2>(tex_.view, [=](ImageView& view, ivec2 pc) {
                        vec2 center = vec2(view.dim()) / 2.f;
                        vec2 pixel_center = vec2(pc) + 0.5f;
                        vec2 offset = pixel_center - center;
                        F32 angle = atan2f(offset.y, offset.x) + glm::pi<F32>();
                        vec4 pixel_norm = convert_colorspace<Colorspace::bt709_linear_hsl, Colorspace::srgb>(
                           vec4((angle * effect_scale_ + 2.f * (sin_time_ + 1.f)) / (glm::pi<F32>() * 2.f), 0.5f, 0.5f, 1.f));
                        put(view, pc, pixel_norm);
                     });
                  };
               } else if (demo == "pinwheel-r") {
                  generator_ = [this]() {
                     auto put = put_pixel_norm_func<ivec2>(tex_.view.image());
                     visit_texture_pixels<ivec2>(tex_.view, [=](ImageView& view, ivec2 pc) {
                        vec2 center = vec2(view.dim()) / 2.f;
                        vec2 pixel_center = vec2(pc) + 0.5f;
                        vec2 offset = pixel_center - center;
                        F32 angle = atan2f(offset.y, offset.x) + glm::pi<F32>();
                        vec4 pixel_norm = convert_colorspace<Colorspace::bt709_linear_hsl, Colorspace::srgb>(
                           vec4((angle * effect_scale_ + 2.f * (sin_time_ + 1.f)) / (glm::pi<F32>() * 2.f), 0.5f, 0.5f, 1.f));
                        pixel_norm = vec4(pixel_norm.r);
                        put(view, pc, pixel_norm);
                     });
                  };
               } else {
                  return false;
               }
               return true;
            }))

         (param({ "w", "x" },{ "width" }, "Set the width of the texture.", [this](const S& value) {
               std::istringstream iss(value);
               iss >> dim_.x;
               return true;
            }))
         (param({ "h", "y" },{ "height" }, "Set the height of the texture.", [this](const S& value) {
               std::istringstream iss(value);
               iss >> dim_.y;
               return true;
            }))
         (flag({ "r" },{ "resizable" }, "Make the window resizable.", resizable_))
         (param({ "s" },{ "scale" }, "Set the scale at which to show the texture.", [this](const S& value) {
               std::istringstream iss(value);
               iss >> scale_;
               return true;
            }))

         (param({ "e" }, { "effect-scale" }, "Set the scale for effects (exact meaning depends on demo).", [this](const S& value) {
               std::istringstream iss(value);
               iss >> effect_scale_;
               return true;
            }))
         (param({ "t" }, { "time-scale" }, "Sets the time scale.  Higher numbers mean faster.", [this](const S& value) {
               std::istringstream iss(value);
               iss >> time_scale_;
               return true;
            }))

         (flag({ },{ "linear"}, "Use linear scaling instead of nearest-neighbor.", linear_scaling_))
         (flag({ "a" },{ "animate" }, "Enables animation.", animate_))

         (end_of_options())

         (verbosity_param({ "v" }, { "verbosity" }, "LEVEL", default_log().verbosity_mask()))
         (flag({ "V" }, { "version" }, "Prints version information to standard output.", show_version))

         (param({ "?" }, { "help" }, "OPTION",
            [&](const S& value) {
               show_help = true;
               help_query = value;
            }).default_value(S())
              .allow_options_as_values(true)
              .desc(Cell() << "Outputs this help message.  For more verbose help, use " << fg_yellow << "--help")
              .extra(Cell() << nl << "If " << fg_cyan << "OPTION" << reset
                            << " is provided, the options list will be filtered to show only options that contain that string."))

         (flag({ }, { "help" },
            [&]() {
               proc.verbose(true);
            }).ignore_values(true))

         (exit_code(0, "There were no errors."))
         (exit_code(1, "An unknown error occurred."))
         (exit_code(2, "There was a problem parsing the command line arguments."))
         ;

      proc(argc, argv);

      if (!show_help && !show_version && !generator_) {
         show_help = true;
         show_version = true;
         status_ = 1;
      }

      if (show_version) {
         proc
            (prologue(BE_CORE_VERSION_STRING).query())
            (prologue(BE_GFX_VERSION_STRING).query())
            (license(BE_LICENSE).query())
            (license(BE_COPYRIGHT).query())
            ;
      }

      if (show_help) {
         proc.describe(std::cout, help_query);
      } else if (show_version) {
         proc.describe(std::cout, ids::cli_describe_section_prologue);
         proc.describe(std::cout, ids::cli_describe_section_license);
      }

   } catch (const cli::OptionException& e) {
      status_ = 2;
      be_error() << S(e.what())
         & attr(ids::log_attr_index) << e.raw_position()
         & attr(ids::log_attr_argument) << S(e.argument())
         & attr(ids::log_attr_option) << S(e.option())
         | default_log();
   } catch (const cli::ArgumentException& e) {
      status_ = 2;
      be_error() << S(e.what())
         & attr(ids::log_attr_index) << e.raw_position()
         & attr(ids::log_attr_argument) << S(e.argument())
         | default_log();
   } catch (const Fatal& e) {
      status_ = 2;
      be_error() << "Fatal error while parsing command line!"
         & attr(ids::log_attr_message) << S(e.what())
         & attr(ids::log_attr_trace) << StackTrace(e.trace())
         | default_log();
   } catch (const std::exception& e) {
      status_ = 2;
      be_error() << "Unexpected exception parsing command line!"
         & attr(ids::log_attr_message) << S(e.what())
         | default_log();
   }
}

///////////////////////////////////////////////////////////////////////////////
int TexDemo::operator()() {
   if (status_ != 0) {
      return status_;
   }

   glbinding::Binding::initialize(false);

   try {
      run_();
   } catch (const Fatal& e) {
      status_ = std::max(status_, (I8)1);
      be_error() << "Unexpected fatal error!"
         & attr(ids::log_attr_message) << S(e.what())
         & attr(ids::log_attr_trace) << StackTrace(e.trace())
         | default_log();
   } catch (const std::exception& e) {
      status_ = std::max(status_, (I8)1);
      be_error() << "Unexpected exception!"
         & attr(ids::log_attr_message) << S(e.what())
         | default_log();
   }

   return status_;
}

namespace {

///////////////////////////////////////////////////////////////////////////////
const char* gl_error_name(GLenum error) {
   switch (error) {
      case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM";
      case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE";
      case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION";
      case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY";
      case GL_STACK_UNDERFLOW:               return "GL_STACK_UNDERFLOW";
      case GL_STACK_OVERFLOW:                return "GL_STACK_OVERFLOW";
      default:                               return "Unknown";
   }
}

///////////////////////////////////////////////////////////////////////////////
void check_errors() {
   GLenum error_status;
   while ((error_status = glGetError()) != GL_NO_ERROR) {
      be_warn() << "OpenGL Error: " << gl_error_name(error_status)
         & attr("Trace") << get_stack_trace()
         | default_log();
   }
}

} // ::()

///////////////////////////////////////////////////////////////////////////////
void TexDemo::run_() {
   glfwWindowHint(GLFW_RESIZABLE, resizable_ ? 1 : 0);
   GLFWwindow* wnd = glfwCreateWindow((int)(dim_.x * scale_), (int)(dim_.y * scale_), "be::gfx::tex Demo", nullptr, nullptr);
   glfwMakeContextCurrent(wnd);
   glfwSwapInterval(1);
   glfwSetWindowUserPointer(wnd, this);

   tex_ = make_planar_texture(format_, dim_, 1);
   rnd_.seed(perf_now());
   
   glClearColor(0.0, 0.0, 0.0, 0.0);
   check_errors();

   glGenTextures(1, &tex_id_);
   glBindTexture(GL_TEXTURE_2D, tex_id_);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear_scaling_ ? GL_LINEAR : GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear_scaling_ ? GL_LINEAR : GL_NEAREST);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
   check_errors();

   if (generator_) {
      generator_();
      check_errors();
   }

   upload_();

   glfwSetWindowSizeCallback(wnd, [](GLFWwindow* wnd, int w, int h) {
      TexDemo& demo = *static_cast<TexDemo*>(glfwGetWindowUserPointer(wnd));
      ivec2 new_size((int)round(w / demo.scale_), (int)round(h / demo.scale_));

      ivec2 new_wnd_size = ivec2(vec2(new_size) * demo.scale_);
      ivec2 old_wnd_size;
      glfwGetWindowSize(wnd, &old_wnd_size.x, &old_wnd_size.y);
      if (old_wnd_size != new_wnd_size) {
         glfwSetWindowSize(wnd, new_wnd_size.x, new_wnd_size.y);
      }
      glViewport(0, 0, new_wnd_size.x, new_wnd_size.y);

      if (new_size != demo.dim_) {
         demo.dim_ = new_size;
         demo.tex_ = make_planar_texture(demo.format_, demo.dim_, 1);
         if (demo.generator_) {
            demo.generator_();
            check_errors();
         }
         demo.upload_();
         glfwPostEmptyEvent();
      }
   });

   glEnable(GL_TEXTURE_2D);
   glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
   check_errors();

   while (!glfwWindowShouldClose(wnd)) {
      if (animate_) {
         glfwPollEvents();
      } else {
         glfwWaitEvents();
      }
      
      glClear(GL_COLOR_BUFFER_BIT);
      check_errors();

      if (animate_ && generator_) {
         time_ = tu_to_seconds(ts_now()) / time_scale_;
         sin_time_ = (F32)sin(time_ * 2.0 * glm::pi<F64>());
         generator_();
         check_errors();
         upload_();
      }

      glBegin(GL_QUADS);
      glColor3fv(glm::value_ptr(glm::vec3(1.f)));
      glTexCoord2fv(glm::value_ptr(glm::vec2(0.0f, 0.f))); glVertex2f(-1.f, 1.f);
      glTexCoord2fv(glm::value_ptr(glm::vec2(1.0f, 0.f))); glVertex2f(1.f, 1.f);
      glTexCoord2fv(glm::value_ptr(glm::vec2(1.0f, 1.f))); glVertex2f(1.f, -1.f);
      glTexCoord2fv(glm::value_ptr(glm::vec2(0.0f, 1.f))); glVertex2f(-1.f, -1.f);
      glEnd();
      check_errors();

      glfwSwapBuffers(wnd);
   }

   glDeleteTextures(1, &tex_id_);
   glfwDestroyWindow(wnd);
}

///////////////////////////////////////////////////////////////////////////////
void TexDemo::upload_() {
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_.view.dim(0).x, tex_.view.dim(0).y, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_.view.image().data());
   check_errors();
}
