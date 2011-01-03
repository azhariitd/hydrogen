/*
 * Hydrogen
 * Copyright(c) 2002-2008 by Alex >Comix< Cominu [comix@users.sourceforge.net]
 *
 * http://www.hydrogen-music.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <hydrogen/basics/drumkit.h>

#include <hydrogen/config.h>
#ifdef H2CORE_HAVE_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#else
	#ifndef WIN32
		#include <zlib.h>
		#include <libtar.h>
	#endif
#endif

#include <hydrogen/basics/sample.h>
#include <hydrogen/basics/instrument.h>
#include <hydrogen/basics/instrument_layer.h>

#include <hydrogen/helpers/xml.h>
#include <hydrogen/helpers/filesystem.h>

namespace H2Core
{
    
const char* Drumkit::__class_name = "Drumkit";

Drumkit::Drumkit() : Object( __class_name ), __samples_loaded(false), __instruments( 0 ) { }

Drumkit::Drumkit( Drumkit* other ) :
    Object( __class_name ),
    __path( other->get_path() ),
    __name( other->get_name() ),
    __author( other->get_author() ),
    __info( other->get_info() ),
    __license( other->get_license() ),
    __samples_loaded( other->samples_loaded() ) {
    __instruments = new InstrumentList( other->get_instruments() );
}

Drumkit::~Drumkit() { if(__instruments) delete __instruments; }

Drumkit* Drumkit::load( const QString& dk_dir ) {
    INFOLOG( QString("Load drumkit %1").arg(dk_dir) );
    if( !Filesystem::drumkit_valid( dk_dir ) ) {
        ERRORLOG( QString("%1 is not valid drumkit").arg(dk_dir) );
        return 0;
    }
    return load_file( Filesystem::drumkit_file(dk_dir) );
}

Drumkit* Drumkit::load_file( const QString& dk_path ) {
    XMLDoc doc;
    if( !doc.read( dk_path, Filesystem::drumkit_xsd() ) ) return 0;
    XMLNode root = doc.firstChildElement( "drumkit_info" );
    if ( root.isNull() ) {
        ERRORLOG( "drumkit_info node not found" );
        return 0;
    }
    Drumkit* drumkit = Drumkit::load_from( &root );
    drumkit->set_path( dk_path );
    return drumkit;
}

Drumkit* Drumkit::load_from( XMLNode* node ) {
    QString drumkit_name = node->read_string( "name", "", false, false );
    if ( drumkit_name.isEmpty() ) {
        ERRORLOG( "Drumkit has no name, abort" );
        return 0;
    }
    Drumkit *drumkit = new Drumkit();
    drumkit->__name = drumkit_name;
    drumkit->__author = node->read_string( "author", "undefined author" );
    drumkit->__info = node->read_string( "info", "defaultInfo" );
    drumkit->__license = node->read_string( "license", "undefined license" );
    drumkit->__samples_loaded = false;
    XMLNode instruments_node = node->firstChildElement( "instrumentList" );
    if ( instruments_node.isNull() ) {
        WARNINGLOG( "instrumentList node not found" );
        drumkit->set_instruments( new InstrumentList() );
    } else {
        drumkit->set_instruments( InstrumentList::load_from( &instruments_node ) );
    }
    return drumkit;
}

bool Drumkit::load_samples( ) {
    INFOLOG( QString("Loading drumkit %1 instrument samples").arg(__name) );
    if(__samples_loaded) return true;
    if( __instruments->load_samples( __path.left( __path.lastIndexOf("/") ) ) ) {
        __samples_loaded = true;
        return true;
    }
    return false;
}

bool Drumkit::unload_samples( ) {
    INFOLOG( QString("Unloading drumkit %1 instrument samples").arg(__name) );
    if(!__samples_loaded) return true;
    if( __instruments->unload_samples() ) {
        __samples_loaded = false;
        return true;
    }
    return false;
}

bool Drumkit::save( const QString& name, const QString& author, const QString& info, const QString& license, InstrumentList* instruments, bool overwrite ) {
    Drumkit *drumkit = new Drumkit();
    drumkit->set_name( name );
    drumkit->set_author( author );
    drumkit->set_info( info );
    drumkit->set_license( license );
	drumkit->set_instruments( instruments );
    bool ret = drumkit->save( overwrite );
	drumkit->set_instruments( 0 );
	delete drumkit;
    return ret;
}

bool Drumkit::save( bool overwrite ) {
    INFOLOG( "Saving drumkit" );
    if( Filesystem::drumkit_exists( __name ) && !overwrite ) {
        ERRORLOG( QString("drumkit %1 already exists").arg(__name) );
        return false;
    }
    QString dk_dir = Filesystem::usr_drumkits_dir() + "/" + __name;
    if( !Filesystem::mkdir( dk_dir ) ) {
        ERRORLOG( QString("unable to create %1").arg(dk_dir) );
        return false;
    }
    bool ret = save_file( Filesystem::drumkit_file(dk_dir), overwrite );
    if ( ret) {
        ret = save_samples( dk_dir, overwrite );
    }
    return ret;
}

bool Drumkit::save_file( const QString& dk_path, bool overwrite ) {
    INFOLOG( QString("Saving drumkit into %1").arg(dk_path) );
    if( Filesystem::file_exists( dk_path, true ) && !overwrite ) {
        ERRORLOG( QString("drumkit %1 already exists").arg(dk_path) );
        return false;
    }
    XMLDoc doc;
    QDomProcessingInstruction header = doc.createProcessingInstruction( "xml", "version=\"1.0\" encoding=\"UTF-8\"");
    doc.appendChild( header );
    XMLNode root = doc.createElement( "drumkit_info" );
    QDomElement el = root.toElement();
    el.setAttribute("xmlns",XMLNS_BASE"/drumkit");
    el.setAttribute("xmlns:xsi",XMLNS_XSI);
    save_to( &root );
    doc.appendChild( root );
    return doc.write( dk_path );
}

void Drumkit::save_to( XMLNode* node ) {
    node->write_string( "name", __name );
    node->write_string( "author", __author );
    node->write_string( "info", __info );
    node->write_string( "license", __license );
    __instruments->save_to( node );
}

bool Drumkit::save_samples( const QString& dk_dir, bool overwrite ) {
    INFOLOG( QString("Saving drumkit %1 samples into %2").arg(dk_dir) );
    if( !Filesystem::mkdir( dk_dir ) ) {
        ERRORLOG( QString("unable to create %1").arg(dk_dir) );
        return false;
    }
    InstrumentList *instruments = get_instruments();
    for( int i=0; i<instruments->size(); i++) {
        Instrument* instrument = (*instruments)[i];
        for ( int n = 0; n < MAX_LAYERS; n++ ) {
            InstrumentLayer* layer = instrument->get_layer(n);
            if(layer) {
                QString src = __path.left( __path.lastIndexOf("/") ) + "/" + layer->get_sample()->get_filename();
                QString dst = dk_dir + "/" + layer->get_sample()->get_filename();
                if( !Filesystem::file_copy( src, dst ) ) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool Drumkit::remove( const QString& sDrumkitName ) {
    QString path = Filesystem::usr_drumkits_dir() + "/" + sDrumkitName;
    if( !Filesystem::drumkit_valid( path ) ) {
        ERRORLOG( QString("%1 is not valid drumkit").arg(path));
        return false;
    }
    _INFOLOG( QString("Removing drumkit: %1").arg(path) );
    if( !Filesystem::rm( path, true ) ) {
        _ERRORLOG( QString("Unable to remove drumkit: %1").arg(path) );
        return false;
    }
    return true;
}

void Drumkit::dump() {
	DEBUGLOG( "Drumkit dump" );
	DEBUGLOG( " |- Path = " + __path );
	DEBUGLOG( " |- Name = " + __name );
	DEBUGLOG( " |- Author = " + __author );
	DEBUGLOG( " |- Info = " + __info );
	DEBUGLOG( " |- Instrument list" );
	for ( int i=0; i<__instruments->size(); i++ ) {
		Instrument* instrument = (*__instruments)[i];          // TODO why do I need an excplicit type conversion
		DEBUGLOG( QString("  |- (%1 of %2) Name = %3")
			 .arg( i )
			 .arg( __instruments->size()-1 )
			 .arg( instrument->get_name() )
			);
		for ( int j=0; j<MAX_LAYERS; j++ ) {
			InstrumentLayer *layer = instrument->get_layer( j );
			if ( layer ) {
				Sample *sample = layer->get_sample();
				if ( sample ) {
					DEBUGLOG( "   |- " + sample->get_filename() );
				} else {
					DEBUGLOG( "   |- NULL sample" );
				}
			} /*else {
				DEBUGLOG( "   |- NULL Layer" );
			}*/

		}
	}
}

#ifdef H2CORE_HAVE_LIBARCHIVE
bool Drumkit::install( const QString& filename ) {
    _INFOLOG( QString("drumkit = ").arg(filename) );
    
    int r;
    struct archive *drumkitFile;
    struct archive_entry *entry;
    char newpath[1024];
    
    drumkitFile = archive_read_new();
    archive_read_support_compression_all(drumkitFile);
    archive_read_support_format_all(drumkitFile);
    if (( r = archive_read_open_file(drumkitFile, filename.toLocal8Bit(), 10240) )) {
        _ERRORLOG( QString( "Error: %2, Could not open drumkit: %1" )
                .arg( archive_errno(drumkitFile))
                .arg( archive_error_string(drumkitFile)) );
        archive_read_close(drumkitFile);
        archive_read_finish(drumkitFile);
        return false;
    }
    QString dk_dir = Filesystem::usr_drumkits_dir() + "/";
    while ( (r = archive_read_next_header(drumkitFile, &entry)) != ARCHIVE_EOF) {
        if (r != ARCHIVE_OK) {
            _ERRORLOG( QString( "Error reading drumkit file: %1").arg(archive_error_string(drumkitFile)));
            break;
        }
        // insert data directory prefix
        QString np = dk_dir + archive_entry_pathname(entry);
        strcpy( newpath, np.toLocal8Bit() );
        archive_entry_set_pathname(entry, newpath);
        // extract tarball
        r = archive_read_extract(drumkitFile, entry, 0);
        if (r == ARCHIVE_WARN) {
            _WARNINGLOG( QString( "warning while extracting %1 (%2)").arg(filename).arg(archive_error_string(drumkitFile)));
        } else if (r != ARCHIVE_OK) {
            _ERRORLOG( QString( "error while extracting %1 (%2)").arg(filename).arg(archive_error_string(drumkitFile)));
            break;
        }
    }
    archive_read_close(drumkitFile);
    archive_read_finish(drumkitFile);
    return true;
}
#else //use libtar
#ifndef WIN32
bool Drumkit::install( const QString& filename ) {
    _INFOLOG( "drumkit = " + filename );
    // GUNZIP !!!
    QString gunzippedName = filename.left( filename.indexOf( "." ) );
    gunzippedName += ".tar";
    FILE *pGunzippedFile = fopen( gunzippedName.toLocal8Bit(), "wb" );
    gzFile gzipFile = gzopen( filename.toLocal8Bit(), "rb" );
    if ( !gzipFile ) {	
        _ERRORLOG( QString( "Error reading drumkit file: %1").arg(filename));
        return false;
    }
    
    uchar buf[4096];
    while ( gzread( gzipFile, buf, 4096 ) > 0 ) {
        fwrite( buf, sizeof( uchar ), 4096, pGunzippedFile );
    }
    gzclose( gzipFile );
    fclose( pGunzippedFile );
    
    // UNTAR !!!
    TAR *tarFile;
    char tarfilename[1024];
    strcpy( tarfilename, gunzippedName.toLocal8Bit() );
    
    if ( tar_open( &tarFile, tarfilename, NULL, O_RDONLY, 0, TAR_VERBOSE | TAR_GNU ) == -1 ) { 
        _ERRORLOG( QString( "tar_open(): %1" ).arg( QString::fromLocal8Bit(strerror(errno)) ) );
        return false;
    }
    char destDir[1024];
    QString dk_dir = Filesystem::usr_drumkits_dir() + "/";
    strcpy( destDir, dataDir.toLocal8Bit() );
    if ( tar_extract_all( tarFile, destDir ) != 0 ) {
        _ERRORLOG( QString( "tar_extract_all(): %1" ).arg( QString::fromLocal8Bit(strerror(errno)) ) );
    }
    if ( tar_close( tarFile ) != 0 ) {
        _ERRORLOG( QString( "tar_close(): %1" ).arg( QString::fromLocal8Bit(strerror(errno)) ) );
    }
    return true;
}
#endif

#endif

};

/* vim: set softtabstop=4 expandtab: */
