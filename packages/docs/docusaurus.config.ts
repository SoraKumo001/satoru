import {themes as prismThemes} from 'prism-react-renderer';
import type {Config} from '@docusaurus/types';
import type * as Preset from '@docusaurus/preset-classic';

const config: Config = {
  title: 'Satoru',
  tagline: 'High-Performance HTML to SVG/PNG/PDF Engine',
  favicon: 'img/favicon.ico',

  future: {
    v4: true,
  },

  // GitHub Pages deployment settings
  url: 'https://sorakumo001.github.io',
  baseUrl: process.env.DOCUSAURUS_BASE_URL || '/satoru/',

  organizationName: 'SoraKumo001',
  projectName: 'satoru',

  onBrokenLinks: 'throw',

  i18n: {
    defaultLocale: 'ja',
    locales: ['ja'],
  },

  presets: [
    [
      'classic',
      {
        docs: {
          sidebarPath: './sidebars.ts',
          editUrl: 'https://github.com/SoraKumo001/satoru/tree/master/packages/docs/',
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      } satisfies Preset.Options,
    ],
  ],

  markdown: {
    mermaid: true,
    hooks: {
      onBrokenMarkdownLinks: 'throw',
    },
  },
  themes: ['@docusaurus/theme-mermaid'],

  themeConfig: {
    image: 'img/docusaurus-social-card.jpg',
    colorMode: {
      defaultMode: 'dark',
      respectPrefersColorScheme: true,
    },
    navbar: {
      title: 'Satoru',
      logo: {
        alt: 'Satoru Logo',
        src: 'img/logo.svg',
      },
      items: [
        {
          type: 'docSidebar',
          sidebarId: 'docsSidebar',
          position: 'left',
          label: 'ドキュメント',
        },
        {
          href: 'https://sorakumo001.github.io/satoru/master',
          label: 'Playground',
          position: 'left',
        },
        {
          href: 'https://github.com/SoraKumo001/satoru',
          label: 'GitHub',
          position: 'right',
        },
      ],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: 'ドキュメント',
          items: [
            {
              label: '概要',
              to: '/docs/overview',
            },
            {
              label: 'アーキテクチャ',
              to: '/docs/architecture',
            },
            {
              label: '互換性',
              to: '/docs/compatibility',
            },
          ],
        },
        {
          title: 'リンク',
          items: [
            {
              label: 'Playground',
              href: 'https://sorakumo001.github.io/satoru/master',
            },
            {
              label: 'npm (satoru-render)',
              href: 'https://www.npmjs.com/package/satoru-render',
            },
          ],
        },
        {
          title: 'その他',
          items: [
            {
              label: 'GitHub',
              href: 'https://github.com/SoraKumo001/satoru',
            },
          ],
        },
      ],
      copyright: `Copyright © ${new Date().getFullYear()} SoraKumo. Built with Docusaurus.`,
    },
    prism: {
      theme: prismThemes.github,
      darkTheme: prismThemes.dracula,
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
