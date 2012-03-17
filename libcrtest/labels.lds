SECTIONS {
	. = ALIGN(8);
	LABELs : {
		labels = . ;
		*(.LABELs);
		___labels_end = . ;
	}
}
