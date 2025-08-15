#include "Types.h"

using namespace std;

namespace Language {

    string typeToString( const Type type ) {
        if ((type & NS) != 0) {
            switch ( type & TypeValueMask ) {
                case TypeNull : return( "Null" );
                case TypeInteger : return( "Integer" );
                case TypeReal : return( "Real" );
                case TypeProcedure : return( "Procedure" );
                case TypeIntrinsic : return( "Intrinsic" );
                case TypeList : return( "List" );
                case TypeSet : return( "Set" );
                case TypeMap : return( "Map" );
                case TypeRecord : return( "Record" );
                case TypeAddress : return( "Address" );
                default : return( "Unknown type");
            }
        } else {
            return( "String[" + to_string( type & TypeValueMask ) + "]" );
        }
    }

} // namespace Language