<?php
require( PHPDOC_INCLUDE_DIR . "exceptions/PhpdocError.php" );

// Phpdoc Core 
require( PHPDOC_INCLUDE_DIR . "core/PhpdocObject.php" );
require( PHPDOC_INCLUDE_DIR . "core/PhpdocArgvHandler.php" );
require( PHPDOC_INCLUDE_DIR . "core/PhpdocSetupHandler.php" );
require( PHPDOC_INCLUDE_DIR . "core/Phpdoc.php" );

// Phpdoc Warning container
require( PHPDOC_INCLUDE_DIR . "warning/PhpdocWarning.php" );

// Phpdoc File Handler
require( PHPDOC_INCLUDE_DIR . "filehandler/PhpdocFileHandler.php" );

// Phpdoc Parser
require( PHPDOC_INCLUDE_DIR . "parser/PhpdocParserRegExp.php" );
require( PHPDOC_INCLUDE_DIR . "parser/PhpdocParserTags.php" );
require( PHPDOC_INCLUDE_DIR . "parser/PhpdocParserCore.php" );
require( PHPDOC_INCLUDE_DIR . "parser/PhpdocUseParser.php" );
require( PHPDOC_INCLUDE_DIR . "parser/PhpdocConstantParser.php" );
require( PHPDOC_INCLUDE_DIR . "parser/PhpdocModuleParser.php" );
require( PHPDOC_INCLUDE_DIR . "parser/PhpdocVariableParser.php" );
require( PHPDOC_INCLUDE_DIR . "parser/PhpdocFunctionParser.php" );
require( PHPDOC_INCLUDE_DIR . "parser/PhpdocClassParser.php" );
require( PHPDOC_INCLUDE_DIR . "parser/PhpdocParser.php" );

// Phpdoc Analyser
require( PHPDOC_INCLUDE_DIR . "analyser/PhpdocAnalyser.php" );
require( PHPDOC_INCLUDE_DIR . "analyser/PhpdocClassAnalyser.php" );
require( PHPDOC_INCLUDE_DIR . "analyser/PhpdocModuleAnalyser.php" );

// Phpdoc Indexer
require( PHPDOC_INCLUDE_DIR . "indexer/PhpdocIndexer.php" );

// Phpdoc XML Writer
require( PHPDOC_INCLUDE_DIR . "xmlwriter/PhpdocXMLWriter.php" );

// Phpdoc XML Exporter
require( PHPDOC_INCLUDE_DIR . "xmlexporter/PhpdocXMLExporter.php" );
require( PHPDOC_INCLUDE_DIR . "xmlexporter/PhpdocXMLIndexExporter.php" );
require( PHPDOC_INCLUDE_DIR . "xmlexporter/PhpdocXMLWarningExporter.php" );
require( PHPDOC_INCLUDE_DIR . "xmlexporter/PhpdocXMLDocumentExporter.php");
require( PHPDOC_INCLUDE_DIR . "xmlexporter/PhpdocXMLModuleExporter.php" );
require( PHPDOC_INCLUDE_DIR . "xmlexporter/PhpdocXMLClassExporter.php" );

// Redistributed IT[X] Templates from the PHPLib
require( PHPDOC_INCLUDE_DIR . "redist/IT.php" );
require( PHPDOC_INCLUDE_DIR . "redist/ITX.php" );

// XML Reader
require( PHPDOC_INCLUDE_DIR . "xmlreader/PhpdocXMLReader.php" );

// API to access XML data
require( PHPDOC_INCLUDE_DIR . "accessor/PhpdocAccessor.php" );
require( PHPDOC_INCLUDE_DIR . "accessor/PhpdocIndexAccessor.php" );
require( PHPDOC_INCLUDE_DIR . "accessor/PhpdocWarningAccessor.php" );
require( PHPDOC_INCLUDE_DIR . "accessor/PhpdocDocumentAccessor.php" );
require( PHPDOC_INCLUDE_DIR . "accessor/PhpdocClassAccessor.php" );
require( PHPDOC_INCLUDE_DIR . "accessor/PhpdocModuleAccessor.php" );

// Phpdoc Renderer
require( PHPDOC_INCLUDE_DIR . "renderer/PhpdocRendererObject.php" );
require( PHPDOC_INCLUDE_DIR . "renderer/html/PhpdocHTMLRenderer.php" );
require( PHPDOC_INCLUDE_DIR . "renderer/html/PhpdocHTMLIndexRenderer.php" );
require( PHPDOC_INCLUDE_DIR . "renderer/html/PhpdocHTMLWarningRenderer.php" );
require( PHPDOC_INCLUDE_DIR . "renderer/html/PhpdocHTMLDocumentRenderer.php" );
require( PHPDOC_INCLUDE_DIR . "renderer/html/PhpdocHTMLModuleRenderer.php" );
require( PHPDOC_INCLUDE_DIR . "renderer/html/PhpdocHTMLClassRenderer.php" );
require( PHPDOC_INCLUDE_DIR . "renderer/html/PhpdocHTMLRendererManager.php" );
?>
