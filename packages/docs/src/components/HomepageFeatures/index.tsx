import type {ReactNode} from 'react';
import Heading from '@theme/Heading';

type FeatureItem = {
  title: string;
  icon: string;
  description: ReactNode;
};

const FeatureList: FeatureItem[] = [
  {
    title: 'Wasm 駆動の高性能レイアウト',
    icon: '⚡',
    description: (
      <>
        <strong>Skia Graphics Engine</strong> と <strong>litehtml</strong> を WebAssembly に統合。
        ブラウザ DOM やヘッドレスブラウザに依存せず、サーバーサイドや Edge、Cloudflare Workers 等の制限された環境で高速に動作します。
      </>
    ),
  },
  {
    title: '広範な CSS & レイアウトサポート',
    icon: '🎨',
    description: (
      <>
        Flexbox や Grid Layout（基本機能）に加え、Float や Box-shadow、コンテナクエリ、メディアクエリ（<code>@media print</code>等）など、本格的な CSS レイアウトを再現可能です。
      </>
    ),
  },
  {
    title: '多様な出力フォーマット',
    icon: '📄',
    description: (
      <>
        SVG（ベクター画像）だけでなく、高品位な PNG、WebP、さらには複数ページに対応した PDF 出力まで、標準でサポートしています。
      </>
    ),
  },
  {
    title: '多言語・自動フォント解決',
    icon: '🌐',
    description: (
      <>
        Unicode サービスと HarfBuzz による高度なテキストシェイピング。CJK（日中韓）などの混在テキストや改行ルールに対応し、Google Fonts からの自動フォントロードもサポートします。
      </>
    ),
  },
  {
    title: 'React 統合 & JSDOM ハイドレーション',
    icon: '⚛️',
    description: (
      <>
        React コンポーネントを直接 HTML に変換して描画できるほか、JSDOM プラグインを用いることで Next.js 等の複雑な SPA のハイドレーション完了後をキャプチャ可能です。
      </>
    ),
  },
  {
    title: 'ステート永続化による高速レンダリング',
    icon: '💾',
    description: (
      <>
        一度計算したレイアウトステートをバイナリとしてシリアライズして永続化（Layout-Once, Render-Anywhere）できます。次回以降の再フロー処理をスキップし、超高速描画を実現します。
      </>
    ),
  },
];

function Feature({title, icon, description}: FeatureItem) {
  return (
    <div className="feature-card">
      <div className="feature-icon-wrapper">
        <span>{icon}</span>
      </div>
      <Heading as="h3" className="feature-title">{title}</Heading>
      <p className="feature-description">{description}</p>
    </div>
  );
}

export default function HomepageFeatures(): ReactNode {
  return (
    <section className="features-section">
      <div className="features-grid">
        {FeatureList.map((props, idx) => (
          <Feature key={idx} {...props} />
        ))}
      </div>
    </section>
  );
}
