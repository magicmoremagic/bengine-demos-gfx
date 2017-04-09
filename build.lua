demo 'd-gfx' {
   app '-tex' {
      icon 'icon/bengine.ico',
      src 'src/tex.cpp',
      link_project {
          'gfx-tex',
          'gfx',
          'platform',
          'core-id'
      }
   }
}
