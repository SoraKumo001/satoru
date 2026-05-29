import type {ReactNode} from 'react';
import Link from '@docusaurus/Link';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import Layout from '@theme/Layout';
import HomepageFeatures from '@site/src/components/HomepageFeatures';
import Heading from '@theme/Heading';

function HomepageHeader() {
  const {siteConfig} = useDocusaurusContext();
  return (
    <header className="hero-banner">
      <div className="container text--center">
        <Heading as="h1" className="hero-title">
          {siteConfig.title}
        </Heading>
        <p className="hero-subtitle">{siteConfig.tagline}</p>
        <div className="hero-buttons">
          <Link
            className="btn-primary-gradient"
            to="/docs/overview">
            ドキュメントを読む 🚀
          </Link>
          <Link
            className="btn-secondary-outline"
            to="https://sorakumo001.github.io/satoru/master">
            Playground デモを試す
          </Link>
        </div>
      </div>
    </header>
  );
}

export default function Home(): ReactNode {
  const {siteConfig} = useDocusaurusContext();
  return (
    <Layout
      title={`${siteConfig.title} - ${siteConfig.tagline}`}
      description="WebAssembly-powered high-performance HTML to SVG/PNG/PDF rendering engine. Fully supports CSS flexbox, grids, gradients, and custom web fonts.">
      <HomepageHeader />
      <main>
        <HomepageFeatures />
      </main>
    </Layout>
  );
}
