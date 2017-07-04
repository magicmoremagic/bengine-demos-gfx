demo 'd-gfx' {
   app '-tex' {
      icon 'icon/bengine.ico',
      limp_src 'src-tex/*.hpp',
      src 'src-tex/tex*.cpp',
      define 'GLM_ENABLE_EXPERIMENTAL',
      link_project {
          'gfx-tex',
          'gfx',
          'platform',
          'core-id',
          'util-fs'
      }
   }
}
