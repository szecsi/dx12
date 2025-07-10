uint hhash(uint morton) {
	//return (morton * 499) % 1103; //testing if collisions work
	// % 32749; // to fit in 32k
	//% 33391; // to exceed 32k

	//return (morton * 499) % 1103;
	return (morton * 499) % 32749;
	//return morton;
}