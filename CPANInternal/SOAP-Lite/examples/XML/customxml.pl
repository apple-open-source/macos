#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite;

my $d = SOAP::Custom::XML::Deserializer
  -> deserialize(join '', <DATA>)
  -> valueof('/Envelope/Body');

foreach my $portfolio ($d->Report->Request->Composition->PortfolioDistribution) {
  print $portfolio->type, " ", $portfolio->date, "\n";
  foreach my $row ($portfolio->Row) {
    print "  ", $row->Element, " ", $row->Value, "\n";
  }
}

__DATA__
<?xml version="1.0" encoding="UTF-8"?>
<Envelope version="1.1">
  <Header />
  <Body>
    <Report>
      <Header>
        <ClientRef />
        <FundCusip>61744J366</FundCusip>
        <SepAcctDesc />
      </Header>
      <Request>
        <Errors>
          <Error>Returned no data for request: PortfolioDistribution</Error>
          <Error>Returned no data for request: PortfolioDistribution</Error>
          <Error>Returned no data for request: PortfolioDistribution</Error>
          <Error>Returned no data for request: PortfolioDistribution</Error>
          <Error>Could not retrieve PortfolioDistribution</Error>
          <Error>Could not retrieve PortfolioDistribution</Error>
          <Error>Could not retrieve PortfolioDistribution</Error>
          <Error>Could not retrieve PortfolioDistribution</Error>
        </Errors>
        <Composition>
          <PortfolioDistribution type="CE" date="09/30/2000" />
          <PortfolioDistribution type="GB" date="09/30/2000" />
          <PortfolioDistribution type="ST" date="09/30/2000">
            <Row>
              <Element>Common Stocks</Element>
              <Value>0.9991</Value>
            </Row>
            <Row>
              <Element>Other</Element>
              <Value>0.0021</Value>
            </Row>
            <Row>
              <Element>Cash &amp; Cash Equivalents</Element>
              <Value>-0.0012</Value>
            </Row>
          </PortfolioDistribution>
          <PortfolioDistribution type="TT" date="09/30/2000">
            <Row>
              <Element>General Electric Company</Element>
              <Value>0.0458</Value>
            </Row>
            <Row>
              <Element>Cisco Systems Inc</Element>
              <Value>0.033</Value>
            </Row>
            <Row>
              <Element>Microsoft Corporation</Element>
              <Value>0.0263</Value>
            </Row>
            <Row>
              <Element>Exxon Mobil Corp.</Element>
              <Value>0.0263</Value>
            </Row>
            <Row>
              <Element>Pfizer, Inc.</Element>
              <Value>0.0231</Value>
            </Row>
            <Row>
              <Element>Intel Corporation</Element>
              <Value>0.0209</Value>
            </Row>
            <Row>
              <Element>Citigroup Inc</Element>
              <Value>0.02</Value>
            </Row>
            <Row>
              <Element>Emc Corp.</Element>
              <Value>0.0185</Value>
            </Row>
            <Row>
              <Element>American International Group, Inc.</Element>
              <Value>0.0181</Value>
            </Row>
            <Row>
              <Element>Oracle Corporation</Element>
              <Value>0.0172</Value>
            </Row>
          </PortfolioDistribution>
          <PortfolioDistribution type="IB" date="09/30/2000">
            <Row>
              <Element>Pharmaceuticals</Element>
              <Value>0.0941</Value>
            </Row>
            <Row>
              <Element>Communications Equipment</Element>
              <Value>0.0857</Value>
            </Row>
            <Row>
              <Element>Computers &amp; Peripherals</Element>
              <Value>0.0764</Value>
            </Row>
            <Row>
              <Element>Diversified Financials</Element>
              <Value>0.0724</Value>
            </Row>
            <Row>
              <Element>Industrial Conglomerates</Element>
              <Value>0.0581</Value>
            </Row>
            <Row>
              <Element>Diversified Telecommunication Services</Element>
              <Value>0.058</Value>
            </Row>
            <Row>
              <Element>Software</Element>
              <Value>0.056</Value>
            </Row>
            <Row>
              <Element>Other</Element>
              <Value>0.5002</Value>
            </Row>
            <Row>
              <Element>Cash &amp; Cash Equivalents</Element>
              <Value>-0.0012</Value>
            </Row>
          </PortfolioDistribution>
          <PortfolioDistribution type="SB" date="09/30/2000">
            <Row>
              <Element>Information Technology</Element>
              <Value>0.2964</Value>
            </Row>
            <Row>
              <Element>Financials</Element>
              <Value>0.154</Value>
            </Row>
            <Row>
              <Element>Health Care</Element>
              <Value>0.1265</Value>
            </Row>
            <Row>
              <Element>Consumer Discretionary</Element>
              <Value>0.1026</Value>
            </Row>
            <Row>
              <Element>Industrials</Element>
              <Value>0.0874</Value>
            </Row>
            <Row>
              <Element>Telecommunication Services</Element>
              <Value>0.0632</Value>
            </Row>
            <Row>
              <Element>Consumer Staples</Element>
              <Value>0.0575</Value>
            </Row>
            <Row>
              <Element>Other</Element>
              <Value>0.1136</Value>
            </Row>
            <Row>
              <Element>Cash &amp; Cash Equivalents</Element>
              <Value>-0.0012</Value>
            </Row>
          </PortfolioDistribution>
        </Composition>
      </Request>
    </Report>
  </Body>
</Envelope>

