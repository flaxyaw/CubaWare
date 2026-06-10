CREATE TABLE `uploads` (
	`id` text PRIMARY KEY NOT NULL,
	`userId` text NOT NULL,
	`filename` text NOT NULL,
	`original_name` text NOT NULL,
	`size` integer NOT NULL,
	`created_at` integer NOT NULL,
	FOREIGN KEY (`userId`) REFERENCES `users`(`id`) ON UPDATE no action ON DELETE cascade
);
