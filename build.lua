demo 'd-gfx' {
   app '-tex' {
      icon 'icon/bengine.ico',
      src 'src/tex*.cpp',
      define 'GLM_ENABLE_EXPERIMENTAL',
      link_project {
          'gfx-tex',
          'gfx',
          'platform',
          'core-id'
      }
   }
}
