#include "tex_demo.hpp"
#include <be/core/logging.hpp>
#include <be/core/version.hpp>
#include <be/core/stack_trace.hpp>
#include <be/core/alg.hpp>
#include <be/util/keyword_parser.hpp>
#include <be/util/get_file_contents.hpp>
#include <be/gfx/version.hpp>
#include <be/gfx/tex/pixel_access_norm.hpp>
#include <be/gfx/tex/make_texture.hpp>
#include <be/gfx/tex/visit_texture.hpp>
#include <be/gfx/tex/convert_colorspace_static.hpp>
#include <be/gfx/tex/image_format_gl.hpp>
#include <be/gfx/tex/texture_reader.hpp>
#include <be/gfx/tex/blit_pixels.hpp>
#include <be/gfx/tex/log_texture_info.hpp>
#include <be/cli/cli.hpp>
#include <be/gfx/bgl.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/common.hpp>
#include <sstream>
#include <string>

using namespace be;
using namespace be::gfx;
using namespace be::gfx::gl;
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
      bool verbose = false;
      S help_query;

      format_ = canonical_format(GL_SRGB_ALPHA);

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
                          << row << "view" << cell << "Attempt to load the image file specified by " << fg_yellow<< "--file" << reset << " and display it."
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
                  setup_ = [this]() {
                     for (int i = 0; i < 8; ++i) {
                        data_[i] = vec4(fdist_(rnd_), fdist_(rnd_), fdist_(rnd_), 1.f);
                     }
                  };
                  generator_ = [this]() {
                     if (time_ > 1.f) {
                        time_ = 0;
                        for (int i = 0; i < 4; ++i) {
                           data_[i] = data_[4 + i];
                        }
                        for (int i = 4; i < 8; ++i) {
                           data_[i] = vec4(fdist_(rnd_), fdist_(rnd_), fdist_(rnd_), 1.f);
                        }
                     }

                     F32 f = glm::smoothstep(0.f, 1.f, F32(time_));
                     vec4 a = glm::mix(data_[0], data_[4], f);
                     vec4 b = glm::mix(data_[1], data_[5], f);
                     vec4 c = glm::mix(data_[2], data_[6], f);
                     vec4 d = glm::mix(data_[3], data_[7], f);
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
               } else if (demo == "view") {
                  generator_ = [this]() {
                     TextureReader reader;
                     reader.read(file_);
                     Texture tex = reader.texture();
                     gfx::tex::log_texture_info(tex.view, file_.string());
                     //tex_ = std::move(tex);
                     dim_ = tex.view.dim(0);
                     tex_ = make_planar_texture(format_, dim_, 1);
                     auto dest = tex_.view.image(0,0,0);
                     be::gfx::tex::blit_pixels(tex.view.image(0, 0, 0), dest);
                  };
               } else if (demo == "view-na") {
                  generator_ = [this]() {
                     TextureReader reader;
                     reader.read(file_);
                     Texture tex = reader.texture();
                     be::gfx::tex::visit_texture_pixels<ivec2>(tex.view, [](be::gfx::tex::ImageView& v, ivec2 pc) {
                        RGBA p = be::gfx::tex::get_block<RGBA>(v, pc);
                        p.a = 255;
                        be::gfx::tex::put_block(v, pc, p);
                     });
                     dim_ = tex.view.dim(0);
                     tex_ = make_planar_texture(format_, dim_, 1);
                     auto dest = tex_.view.image(0,0,0);
                     be::gfx::tex::blit_pixels(tex.view.image(0, 0, 0), dest);
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
         (flag({ "r" },{ "resizable" }, resizable_).desc("Make the window resizable."))
         (param({ "s" },{ "scale" }, "SCALE", [this](const S& value) {
               std::istringstream iss(value);
               iss >> scale_;
               return true;
            }).desc("Set the scale at which to show the texture."))

         (param({ "f" },{ "format" }, "FORMAT", [this](const S& value) {
               using namespace gl;
               util::KeywordParser<gl::GLenum> parser(GL_SRGB_ALPHA);
               parser

                  /*!! register_template_string([[`with each $ using # { `(GL_`$`, "GL_`$`", "`$`")` nl }]], 'kw')
                  write_template('kw', {
                  'R8',            'R16',      'R8_SNORM',      'R16_SNORM',
                  'R8UI',          'R16UI',    'R32UI',         'R8I',
                  'R16I',          'R32I',     'R16F',          'R32F',
                  'RG8',           'RG16',     'RG8_SNORM',     'RG16_SNORM',
                  'RG8UI',         'RG16UI',   'RG32UI',        'RG8I',
                  'RG16I',         'RG32I',    'RG16F',         'RG32F',
                  'SRGB8',         'RGB8',     'RGB16',         'R3_G3_B2',
                  --[['RGB565',]]  'RGB4',     'RGB5',          'RGB8_SNORM',
                  'RGB16_SNORM',   'RGB8UI',   'RGB16UI',       'RGB32UI',
                  'RGB8I',         'RGB16I',   'RGB32I',        'R11F_G11F_B10F',
                  'RGB16F',        'RGB32F',   'RGB9_E5',       'RGBA16',
                  'RGBA4',         'RGB5_A1',  'RGB10_A2',      'RGBA8_SNORM',
                  'RGBA16_SNORM',  'RGBA8UI',  'RGBA16UI',      'RGBA32UI',
                  'RGB10_A2UI',    'RGBA8I',   'RGBA16I',       'RGBA32I',
                  'RGBA16F',       'RGBA32F',  'SRGB8_ALPHA8',  'RGBA8'
                  }) !! 63 */
                  /* ################# !! GENERATED CODE -- DO NOT MODIFY !! ################# */
                  (GL_R8, "GL_R8", "R8")
                  (GL_R16, "GL_R16", "R16")
                  (GL_R8_SNORM, "GL_R8_SNORM", "R8_SNORM")
                  (GL_R16_SNORM, "GL_R16_SNORM", "R16_SNORM")
                  (GL_R8UI, "GL_R8UI", "R8UI")
                  (GL_R16UI, "GL_R16UI", "R16UI")
                  (GL_R32UI, "GL_R32UI", "R32UI")
                  (GL_R8I, "GL_R8I", "R8I")
                  (GL_R16I, "GL_R16I", "R16I")
                  (GL_R32I, "GL_R32I", "R32I")
                  (GL_R16F, "GL_R16F", "R16F")
                  (GL_R32F, "GL_R32F", "R32F")
                  (GL_RG8, "GL_RG8", "RG8")
                  (GL_RG16, "GL_RG16", "RG16")
                  (GL_RG8_SNORM, "GL_RG8_SNORM", "RG8_SNORM")
                  (GL_RG16_SNORM, "GL_RG16_SNORM", "RG16_SNORM")
                  (GL_RG8UI, "GL_RG8UI", "RG8UI")
                  (GL_RG16UI, "GL_RG16UI", "RG16UI")
                  (GL_RG32UI, "GL_RG32UI", "RG32UI")
                  (GL_RG8I, "GL_RG8I", "RG8I")
                  (GL_RG16I, "GL_RG16I", "RG16I")
                  (GL_RG32I, "GL_RG32I", "RG32I")
                  (GL_RG16F, "GL_RG16F", "RG16F")
                  (GL_RG32F, "GL_RG32F", "RG32F")
                  (GL_SRGB8, "GL_SRGB8", "SRGB8")
                  (GL_RGB8, "GL_RGB8", "RGB8")
                  (GL_RGB16, "GL_RGB16", "RGB16")
                  (GL_R3_G3_B2, "GL_R3_G3_B2", "R3_G3_B2")
                  (GL_RGB4, "GL_RGB4", "RGB4")
                  (GL_RGB5, "GL_RGB5", "RGB5")
                  (GL_RGB8_SNORM, "GL_RGB8_SNORM", "RGB8_SNORM")
                  (GL_RGB16_SNORM, "GL_RGB16_SNORM", "RGB16_SNORM")
                  (GL_RGB8UI, "GL_RGB8UI", "RGB8UI")
                  (GL_RGB16UI, "GL_RGB16UI", "RGB16UI")
                  (GL_RGB32UI, "GL_RGB32UI", "RGB32UI")
                  (GL_RGB8I, "GL_RGB8I", "RGB8I")
                  (GL_RGB16I, "GL_RGB16I", "RGB16I")
                  (GL_RGB32I, "GL_RGB32I", "RGB32I")
                  (GL_R11F_G11F_B10F, "GL_R11F_G11F_B10F", "R11F_G11F_B10F")
                  (GL_RGB16F, "GL_RGB16F", "RGB16F")
                  (GL_RGB32F, "GL_RGB32F", "RGB32F")
                  (GL_RGB9_E5, "GL_RGB9_E5", "RGB9_E5")
                  (GL_RGBA16, "GL_RGBA16", "RGBA16")
                  (GL_RGBA4, "GL_RGBA4", "RGBA4")
                  (GL_RGB5_A1, "GL_RGB5_A1", "RGB5_A1")
                  (GL_RGB10_A2, "GL_RGB10_A2", "RGB10_A2")
                  (GL_RGBA8_SNORM, "GL_RGBA8_SNORM", "RGBA8_SNORM")
                  (GL_RGBA16_SNORM, "GL_RGBA16_SNORM", "RGBA16_SNORM")
                  (GL_RGBA8UI, "GL_RGBA8UI", "RGBA8UI")
                  (GL_RGBA16UI, "GL_RGBA16UI", "RGBA16UI")
                  (GL_RGBA32UI, "GL_RGBA32UI", "RGBA32UI")
                  (GL_RGB10_A2UI, "GL_RGB10_A2UI", "RGB10_A2UI")
                  (GL_RGBA8I, "GL_RGBA8I", "RGBA8I")
                  (GL_RGBA16I, "GL_RGBA16I", "RGBA16I")
                  (GL_RGBA32I, "GL_RGBA32I", "RGBA32I")
                  (GL_RGBA16F, "GL_RGBA16F", "RGBA16F")
                  (GL_RGBA32F, "GL_RGBA32F", "RGBA32F")
                  (GL_SRGB8_ALPHA8, "GL_SRGB8_ALPHA8", "SRGB8_ALPHA8")
                  (GL_RGBA8, "GL_RGBA8", "RGBA8")

                  /* ######################### END OF GENERATED CODE ######################### */
                  ;

               std::error_code ec;
               format_ = canonical_format(parser.parse(value, ec));
               if (ec) {
                  throw RecoverableError(ec);
               }
            }).desc("Set OpenGL internal format."))

          (param({ }, { "file" }, "PATH", [&](const S& value) {
               file_ = value;
            }).desc("Specifies the path to an image file for demos that require an input image."))

         (numeric_param({ "e" }, { "effect-scale" }, "X", effect_scale_).desc("Set the scale for effects (exact meaning depends on demo)."))

         (numeric_param({ "t" }, { "time-scale" }, "X", time_scale_).desc("Sets the time scale.  Higher numbers mean faster."))

         (flag({ }, { "linear" }, linear_scaling_).desc("Use linear scaling instead of nearest-neighbor."))
         (flag({ "a" }, { "animate" }, animate_).desc("Enables animation."))

         (end_of_options())

         (verbosity_param({ "v" }, { "verbosity" }, "LEVEL", default_log().verbosity_mask()))

         (flag({ "V" }, { "version" }, show_version).desc("Prints version information to standard output."))

         (param({ "?" }, { "help" }, "OPTION", [&](const S& value) {
               show_help = true;
               help_query = value;
            }).default_value(S())
               .allow_options_as_values(true)
               .desc(Cell() << "Outputs this help message.  For more verbose help, use " << fg_yellow << "--help")
               .extra(Cell() << nl << "If " << fg_cyan << "OPTION" << reset
                             << " is provided, the options list will be filtered to show only options that contain that string."))

         (flag({ }, { "help" }, verbose).ignore_values(true))

         (exit_code(0, "There were no errors."))
         (exit_code(1, "An unknown error occurred."))
         (exit_code(2, "There was a problem parsing the command line arguments."))
         ;

      proc.process(argc, argv);

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
         proc.describe(std::cout, verbose, help_query);
      } else if (show_version) {
         proc.describe(std::cout, verbose, ids::cli_describe_section_prologue);
         proc.describe(std::cout, verbose, ids::cli_describe_section_license);
      }

   } catch (const cli::OptionError& e) {
      status_ = 2;
      be_error() << S(e.what())
         & attr(ids::log_attr_index) << e.raw_position()
         & attr(ids::log_attr_argument) << S(e.argument())
         & attr(ids::log_attr_option) << S(e.option())
         | default_log();
   } catch (const cli::ArgumentError& e) {
      status_ = 2;
      be_error() << S(e.what())
         & attr(ids::log_attr_index) << e.raw_position()
         & attr(ids::log_attr_argument) << S(e.argument())
         | default_log();
   } catch (const FatalTrace& e) {
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

   try {
      run_();
   } catch (const FatalTrace& e) {
      status_ = std::max(status_, (I8)1);
      be_error() << "Unexpected fatal error!"
         & attr(ids::log_attr_message) << S(e.what())
         & attr(ids::log_attr_trace) << StackTrace(e.trace())
         | default_log();
   } catch (const fs::filesystem_error& e) {
      status_ = std::max(status_, (I8)1);
      be_error() << "Unexpected error!"
         & attr(ids::log_attr_message) << S(e.what())
         & attr(ids::log_attr_category) << e.code().category().name()
         & attr(ids::log_attr_error_code) << e.code().value()
         & attr(ids::log_attr_error) << e.code().message()
         & attr(ids::log_attr_path) << e.path1().generic_string()
         & attr(ids::log_attr_path) << e.path2().generic_string()
         | default_log();
   } catch (const std::system_error& e) {
      status_ = std::max(status_, (I8)1);
      be_error() << "Unexpected error!"
         & attr(ids::log_attr_message) << S(e.what())
         & attr(ids::log_attr_category) << e.code().category().name()
         & attr(ids::log_attr_error_code) << e.code().value()
         & attr(ids::log_attr_error) << e.code().message()
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
void GLAPIENTRY check_errors(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
   be_warn() << "OpenGL Error"
      & attr("Source") << source
      & attr("Type") << type
      & attr("ID") << id
      & attr("Severity") << severity
      & attr("Message") << message
      & attr("Trace") << get_stack_trace()
      | default_log();
}

} // ::()

///////////////////////////////////////////////////////////////////////////////
void TexDemo::run_() {
   glfwWindowHint(GLFW_RESIZABLE, resizable_ ? 1 : 0);
   GLFWwindow* wnd = glfwCreateWindow((int)(dim_.x * scale_), (int)(dim_.y * scale_), "be::gfx::tex Demo", nullptr, nullptr);
   glfwMakeContextCurrent(wnd);
   glfwSwapInterval(1);
   glfwSetWindowUserPointer(wnd, this);

   gl::init_context();

   if (GL_KHR_debug) {
      //#bgl checked(GL_KHR_debug)
      glEnable(GL_DEBUG_OUTPUT);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
      glDebugMessageCallback(check_errors, nullptr);
      //#bgl unchecked
   }

   tex_ = make_planar_texture(format_, dim_, 1);
   rnd_.seed(perf_now());

   glClearColor(0.0, 0.0, 0.0, 0.0);

   glGenTextures(1, &tex_id_);
   glBindTexture(GL_TEXTURE_2D, tex_id_);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear_scaling_ ? GL_LINEAR : GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear_scaling_ ? GL_LINEAR : GL_NEAREST);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 8);

   if (setup_) {
      setup_();
   }

   if (generator_) {
      generator_();
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

      if (new_size != demo.dim_ && new_size.x * new_size.y > 0) {
         demo.dim_ = new_size;
         demo.tex_ = make_planar_texture(demo.format_, demo.dim_, 1);
         if (demo.generator_) {
            demo.generator_();
         }
         demo.upload_();
         glfwPostEmptyEvent();
      }
   });

   glEnable(GL_TEXTURE_2D);
   glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

   while (!glfwWindowShouldClose(wnd)) {
      if (animate_) {
         glfwPollEvents();
      } else {
         glfwWaitEvents();
      }

      glClear(GL_COLOR_BUFFER_BIT);

      if (animate_ && generator_) {
         last_ = now_;
         now_ = ts_now();
         time_ += tu_to_seconds(now_ - last_) / time_scale_;
         sin_time_ = (F32)sin(time_ * 2.0 * glm::pi<F64>());
         generator_();
         upload_();
      }

      glBegin(GL_QUADS);
      glColor3fv(glm::value_ptr(glm::vec3(1.f)));
      glTexCoord2fv(glm::value_ptr(glm::vec2(0.0f, 0.f))); glVertex2f(-1.f, 1.f);
      glTexCoord2fv(glm::value_ptr(glm::vec2(1.0f, 0.f))); glVertex2f(1.f, 1.f);
      glTexCoord2fv(glm::value_ptr(glm::vec2(1.0f, 1.f))); glVertex2f(1.f, -1.f);
      glTexCoord2fv(glm::value_ptr(glm::vec2(0.0f, 1.f))); glVertex2f(-1.f, -1.f);
      glEnd();

      glfwSwapBuffers(wnd);
   }

   glDeleteTextures(1, &tex_id_);
   glfwDestroyWindow(wnd);
}

///////////////////////////////////////////////////////////////////////////////
void TexDemo::upload_() {
   auto f = to_gl_format(tex_.view.format());

   glPixelStorei(GL_UNPACK_ALIGNMENT, (GLint) tex_.storage->line_alignment());

   be_verbose() << "Uploading image"
      & attr("Internal Format") << enum_name(f.internal_format)
      & attr("Data Format") << enum_name(f.data_format)
      & attr("Data Type") << enum_name(f.data_type)
      | default_log();

   //glCompressedTexImage2D(GL_TEXTURE_2D, 0, f.internal_format, tex_.view.dim(0).x, tex_.view.dim(0).y, 0, (GLsizei)tex_.view.image().size(), tex_.view.image().data());

   glTexImage2D(GL_TEXTURE_2D, 0, f.internal_format, tex_.view.dim(0).x, tex_.view.dim(0).y, 0, f.data_format, f.data_type, tex_.view.image().data());
}
