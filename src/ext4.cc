/* Copyright (C) 2008 Curtis Gedak
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
 
#include "../include/ext4.h"

namespace GParted
{

FS ext4::get_filesystem_support()
{
	FS fs ;
	fs .filesystem = GParted::FS_EXT4 ;

	//To be on the safe side, only enable all the function if mkfs.ext4 is
	//  found indicating that there is a recent copy of e2fsprogs available.
	if ( ! Glib::find_program_in_path( "mkfs.ext4" ) .empty() )
	{
		fs .create = GParted::FS::EXTERNAL ;

		if ( ! Glib::find_program_in_path( "dumpe2fs" ) .empty() )
			fs .read = GParted::FS::EXTERNAL ;
	
		if ( ! Glib::find_program_in_path( "e2label" ) .empty() ) {
			fs .read_label = FS::EXTERNAL ;
			fs .write_label = FS::EXTERNAL ;
		}

		if ( ! Glib::find_program_in_path( "e2fsck" ) .empty() )
			fs .check = GParted::FS::EXTERNAL ;

		if ( ! Glib::find_program_in_path( "resize2fs" ) .empty() && fs .check )
		{
			fs .grow = GParted::FS::EXTERNAL ;

			if ( fs .read ) //needed to determine a min file system size..
				fs .shrink = GParted::FS::EXTERNAL ;
		}

		if ( fs .check )
		{
			fs .copy = GParted::FS::GPARTED ;
			fs .move = GParted::FS::GPARTED ;
		}
	}

	return fs ;
}

void ext4::set_used_sectors( Partition & partition ) 
{
	if ( ! Utils::execute_command( "dumpe2fs -h " + partition .get_path(), output, error, true ) )
	{
		index = output .find( "Free blocks:" ) ;
		if ( index >= output .length() ||
		     sscanf( output.substr( index ) .c_str(), "Free blocks: %Ld", &N ) != 1 )   
			N = -1 ;
	
		index = output .find( "Block size:" ) ;
		if ( index >= output.length() || 
		     sscanf( output.substr( index ) .c_str(), "Block size: %Ld", &S ) != 1 )  
			S = -1 ;

		if ( N > -1 && S > -1 )
			partition .Set_Unused( Utils::round( N * ( S / 512.0 ) ) ) ;
	}
	else
	{
		if ( ! output .empty() )
			partition .messages .push_back( output ) ;
		
		if ( ! error .empty() )
			partition .messages .push_back( error ) ;
	}
}

void ext4::read_label( Partition & partition )
{
	if ( ! Utils::execute_command( "e2label " + partition .get_path(), output, error, true ) )
	{
		partition .label = Utils::regexp_label( output, "^(.*)" ) ;
	}
	else
	{
		if ( ! output .empty() )
			partition .messages .push_back( output ) ;
		
		if ( ! error .empty() )
			partition .messages .push_back( error ) ;
	}
}

bool ext4::write_label( const Partition & partition, OperationDetail & operationdetail )
{
	return ! execute_command( "e2label " + partition .get_path() + " \"" + partition .label + "\"", operationdetail ) ;
}

bool ext4::create( const Partition & new_partition, OperationDetail & operationdetail )
{
	return ! execute_command( "mkfs.ext4 -L \"" + new_partition .label + "\" " + new_partition .get_path(), operationdetail ) ;
}

bool ext4::resize( const Partition & partition_new, OperationDetail & operationdetail, bool fill_partition )
{
	Glib::ustring str_temp = "resize2fs " + partition_new .get_path() ;
	
	if ( ! fill_partition )
		str_temp += " " + Utils::num_to_str( Utils::round( Utils::sector_to_unit( 
					partition_new .get_length(), GParted::UNIT_KIB ) ) -1, true ) + "K" ; 
		
	return ! execute_command( str_temp, operationdetail ) ;
}

bool ext4::copy( const Glib::ustring & src_part_path,
		 const Glib::ustring & dest_part_path,
		 OperationDetail & operationdetail )
{
	return true ;
}

bool ext4::check_repair( const Partition & partition, OperationDetail & operationdetail )
{
	exit_status = execute_command( "e2fsck -f -y -v " + partition .get_path(), operationdetail ) ;

	//exitstatus 256 isn't documented, but it's returned when the 'FILE SYSTEM IS MODIFIED'
	//this is quite normal (especially after a copy) so we let the function return true...
	return ( exit_status == 0 || exit_status == 1 || exit_status == 2 || exit_status == 256 ) ;
}
	
} //GParted